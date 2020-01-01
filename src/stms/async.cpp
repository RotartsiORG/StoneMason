//
// Created by grant on 12/30/19.
//

#include <iostream>
#include "stms/async.hpp"

namespace hp2 {
    static void workerFunc(ThreadPool *parent, size_t index) {
        while (parent->running) {
            if (index == parent->stopRequest) {
                parent->stopRequest = 0;
                return;
            }

            parent->taskQueueMtx.lock();
            if (parent->tasks.empty()) {
                parent->taskQueueMtx.unlock();
                std::this_thread::sleep_for(std::chrono::milliseconds(parent->workerDelay));
                continue;
            } else {
                ThreadPoolTask front = parent->tasks.top();
                parent->tasks.pop();
                parent->taskQueueMtx.unlock();

                if (front.pTask == nullptr) {
                    std::cerr << "For a task submitted to a thread pool, `task.pTask` was `nullptr`!";
                    std::cerr << " This would've segfaulted! Ignoring this task!" << std::endl;
                    continue;
                }

                std::packaged_task<void *(void *)> frontTask = std::move(*front.pTask);
                frontTask(front.pData);
            }
        }
    }

    ThreadPool::TaskComparator::TaskComparator(const bool &rev) : reverse(rev) {}

    bool ThreadPool::TaskComparator::operator()(const ThreadPoolTask &lhs, const ThreadPoolTask &rhs) const {
        if (this->reverse) {
            return (lhs.priority > rhs.priority);
        } else {
            return (lhs.priority < rhs.priority);
        }
    }


    void ThreadPool::start(uint32_t threads) {
        if (this->running) {
            std::cerr << "`ThreadPool::start()` was called when thread pool is already running! Ignoring invocation!"
                      << std::endl;
            return;
        }
        if (threads == 0) {
            threads = (uint32_t) (std::thread::hardware_concurrency() * 1); // Choose a good multiplier
            if (threads == 0) { // If that's STILL 0, default to 8 threads.
                threads = 8;
            }
        }

        this->running = true;
        {
            std::lock_guard<std::mutex> lg(this->workerMtx);
            for (uint32_t i = 0; i < threads; i++) {
                this->workers.emplace_back(std::thread(workerFunc, this, i + 1));
            }
        }
    }

    void ThreadPool::stop() {
        if (!this->running) {
            std::cerr << "`ThreadPool::stop()` was called when thread pool is already stopped! Ignoring invocation!"
                      << std::endl;
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

            if (front.joinable()) {
                front.join();
            } else {
                front.detach();  // Simply forget thread if not joinable.
            }
        }
    }

    void ThreadPool::submitTask(ThreadPoolTask task) {
        std::lock_guard<std::mutex> lg(this->taskQueueMtx);
        this->tasks.push(task);
    }

    void ThreadPool::pushWorker() {
        if (!this->running) {
            std::cerr << "`ThreadPool::pushWorker()` called while the thread pool was stopped! Ignoring invocation!"
                      << std::endl;
            return;
        }

        {
            std::lock_guard<std::mutex> lg(this->workerMtx);
            this->workers.emplace_back(workerFunc, this, this->workers.size() + 1);
        }
    }

    void ThreadPool::popWorker() {
        if (!this->running) {
            throw std::runtime_error("A worker was popped while the thread pool was stopped!");
        }

        std::thread back;
        {
            std::lock_guard<std::mutex> lg(this->workerMtx);
            back = std::move(this->workers.back());
            this->stopRequest = this->workers.size(); // Request the last worker to stop.
            this->workers.pop_back();
        }

        if (back.joinable()) {
            back.join();
        } else {
            back.detach();
        }
    }

    ThreadPool &ThreadPool::operator=(ThreadPool &&rhs) noexcept {
        if (&rhs == this) {
            return *this;
        }

        this->~ThreadPool();

        if (rhs.running) {
            std::cerr
                    << "A thread pool was moved (using `std::move`) whilst it was still running! This MAY cause issues!";
            std::cerr
                    << " The mutex controlling access to the task queue cannot be moved with the thread pool, therefore the mutex member is ignored!";
            std::cerr << " This MAY result in a race condition, and you WILL have to debug it!" << std::endl;
        }

        // We cannot move the mutex so we quietly skip it and hope nobody notices. (Watch it crash and burn later)
        this->stopRequest = rhs.stopRequest;
        this->running = rhs.running;
        this->workerDelay = rhs.workerDelay;
        {
            std::lock_guard<std::mutex> rhsLg(rhs.taskQueueMtx);
            std::lock_guard<std::mutex> thisLg(this->taskQueueMtx);
            this->tasks = std::move(rhs.tasks);
        }
        {
            std::lock_guard<std::mutex> rhsLg(rhs.workerMtx);
            std::lock_guard<std::mutex> thisLg(this->workerMtx);
            this->workers = std::move(rhs.workers);
        }

        return *this;
    }

    ThreadPool::ThreadPool(ThreadPool &&rhs) noexcept {
        *this = std::move(rhs);
    }

    ThreadPool::~ThreadPool() {
        if (this->running) {
            stop();
        }
        std::lock_guard<std::mutex> lg(this->taskQueueMtx);
        if (!tasks.empty()) {
            std::cerr << "Warning! Thread pool destroyed with " << tasks.size()
                      << " tasks still incomplete! They will NEVER get executed!"
                      << std::endl;
        }
    }
}
