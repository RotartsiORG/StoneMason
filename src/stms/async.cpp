//
// Created by grant on 12/30/19.
//

#include <iostream>
#include <stms/logging.hpp>
#include "stms/async.hpp"

namespace stms {
    static void workerFunc(ThreadPool *parent, size_t index) {
        while (parent->running) {
            if (index == parent->stopRequest) {
                parent->stopRequest = 0;
                return;
            }

            std::unique_lock<std::mutex> tlg(parent->taskQueueMtx);
            if (parent->tasks.empty()) {
                tlg.unlock();
                std::this_thread::yield();
                std::this_thread::sleep_for(std::chrono::milliseconds(parent->workerDelay));
                continue;
            } else {
                auto front = std::move(parent->tasks.front());
                parent->tasks.pop();
                tlg.unlock();

                front(); // execute the task UwU

                std::lock_guard<std::mutex> lg(parent->unfinishedTaskMtx);
                parent->unfinishedTasks--;
            }
        }
    }


    void ThreadPool::destroy() {
        if (this->running) {
            STMS_WARN("ThreadPool destroyed while running! Stopping it now (with block=true)");
            stop(true);
        }

        if (!tasks.empty()) {
            STMS_WARN("ThreadPool destroyed with unfinished tasks! {} tasks will never be executed!", tasks.size());
        }
    }

    void ThreadPool::start(unsigned threads) {
        if (running) {
            STMS_WARN("ThreadPool::start() called when already started! Ignoring...");
            return;
        }

        if (threads == 0) {
            threads = (unsigned) (std::thread::hardware_concurrency() * 1); // Choose a good multiplier
            if (threads == 0) { // If that's STILL 0, default to 8 threads.
                threads = 8;
            }
        }

        this->running = true;
        {
            std::lock_guard<std::mutex> lg(this->workerMtx);
            for (unsigned i = 0; i < threads; i++) {
                this->workers.emplace_back(std::thread(workerFunc, this, i + 1));
            }
        }
    }

    void ThreadPool::stop(bool block) {
        if (!running) {
            STMS_WARN("ThreadPool::stop() called when already stopped! Ignoring...");
            return;
        }

        this->running = false;

        bool workersEmpty;

        {
            std::lock_guard<std::mutex> lg(this->workerMtx);
            workersEmpty = this->workers.empty();
        }

        while (!workersEmpty) {
            std::thread front;

            {
                std::lock_guard<std::mutex> lg(this->workerMtx);
                front = std::move(this->workers.front());
                this->workers.pop_front();
                workersEmpty = this->workers.empty();
            }

            if (front.joinable() && block) {
                front.join();
            } else {
                front.detach();  // Simply forget thread if not joinable.
            }
        }
    }

    std::future<void> ThreadPool::submitTask(const std::function<void(void)> &func) {
        {
            std::lock_guard<std::mutex> lg(unfinishedTaskMtx);
            unfinishedTasks++;
        }
        auto task = std::packaged_task<void(void)>(func);

        // Save future to variable since `task` is moved.
        auto future = task.get_future();

        std::lock_guard<std::mutex> lg(this->taskQueueMtx);
        this->tasks.emplace(std::move(task));

        return future;
    }

    void ThreadPool::pushThread() {
        if (!this->running) {
            STMS_WARN(
                    "`ThreadPool::pushThread()` called while the thread pool was stopped! Starting the thread pool!");
            this->running = true;
        }

        {
            std::lock_guard<std::mutex> lg(this->workerMtx);
            this->workers.emplace_back(workerFunc, this, this->workers.size() + 1);
        }
    }

    void ThreadPool::popThread(bool block) {
        if (!running) {
            STMS_WARN("ThreadPool::popThread() called when stopped! Ignoring invocation!");
            return;
        }

        std::thread back;
        {
            std::lock_guard<std::mutex> lg(this->workerMtx);
            back = std::move(this->workers.back());
            this->stopRequest = this->workers.size(); // Request the last worker to stop.
            this->workers.pop_back();
            if (this->workers.empty()) {
                STMS_WARN("The last thread was popped from ThreadPool! Stopping the pool!");
                this->running = false;
            }
        }

        if (back.joinable() && block) {
            back.join();
        } else {
            back.detach();
        }
    }

    ThreadPool &ThreadPool::operator=(ThreadPool &&rhs) noexcept {
        if (&rhs == this) {
            return *this;
        }

        this->destroy();

        // Hopefully locking ALL mutexes during the move is enough to prevent aforementioned race conditions.
        std::lock_guard<std::mutex> rhsWorkerLg(rhs.workerMtx);
        std::lock_guard<std::mutex> thisWorkerLg(this->workerMtx);
        std::lock_guard<std::mutex> rhsTaskLg(rhs.taskQueueMtx);
        std::lock_guard<std::mutex> thisTaskLg(this->taskQueueMtx);

        std::lock_guard<std::mutex> thisTaskCountLg(this->unfinishedTaskMtx);
        std::lock_guard<std::mutex> rhsTaskCountLg(rhs.unfinishedTaskMtx);

        // We cannot move the mutex so we quietly skip it and hope nobody notices. (Watch it crash and burn later)
        this->stopRequest = rhs.stopRequest;
        this->running = rhs.running;
        this->workerDelay = rhs.workerDelay;
        this->tasks = std::move(rhs.tasks);
        this->workers = std::move(rhs.workers);
        this->unfinishedTasks = rhs.unfinishedTasks;

        return *this;
    }

    ThreadPool::ThreadPool(ThreadPool &&rhs) noexcept {
        *this = std::move(rhs);
    }

    ThreadPool::~ThreadPool() {
        this->destroy();
    }

    void ThreadPool::waitIdle() {
        while (unfinishedTasks > 0) {
            std::this_thread::yield();
            std::this_thread::sleep_for(std::chrono::milliseconds(workerDelay));
        }
    }
}
