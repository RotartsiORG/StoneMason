//
// Created by grant on 12/30/19.
//

#include <iostream>
#include <stms/logging.hpp>
#include "stms/async.hpp"
#include <stms/util/compare.hpp>

namespace stms {
    static void workerFunc(ThreadPool *parent, size_t index) {
        while (parent->running) {
            if (index == parent->stopRequest) {
                parent->stopRequest = 0;
                return;
            }

            std::unique_lock<std::mutex> tlg(parent->taskQueueMtx);
            // Block until there are tasks to consume or we are requested to stop
            if (parent->tasks.empty()) {
                parent->taskQueueCv.wait_for(tlg, std::chrono::milliseconds(threadPoolConvarTimeoutMs), [&]() {
                    return (!parent->tasks.empty()) || index == parent->stopRequest || (!parent->running);
                });
            } else if (!parent->tasks.empty()) {
                auto front = std::move(parent->tasks.front());
                parent->tasks.pop();
                tlg.unlock();

                front(); // execute the task UwU

                std::lock_guard<std::mutex> lg(parent->unfinishedTaskMtx);
                parent->unfinishedTasks--;
                parent->unfinishedTasksCv.notify_all();
            }
        }
    }


    void ThreadPool::destroy() {
        if (!tasks.empty()) {
            STMS_WARN("ThreadPool destroyed with unfinished tasks! {} tasks will never be executed!", tasks.size());
        }

        if (this->running) {
            STMS_WARN("ThreadPool destroyed while running! Stopping it now (with block=true)");
            waitIdle(1000);
            stop(true);
        }
    }

    void ThreadPool::start(unsigned threads) {
        if (running) {
            STMS_WARN("ThreadPool::start() called when already started! Restarting the thread pool...");
            waitIdle(1000);
            stop(true);
        }

        if (threads == 0) {
            threads = (unsigned) (std::thread::hardware_concurrency() - 1); // subtract 1 bc of the main thread :D
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
        taskQueueCv.notify_all(); // Notify all workers that we are stopped!

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
        auto task = std::packaged_task<void(void)>(func);
        auto future = task.get_future(); // Save future to variable since `task` is moved.
        submitPackagedTask(std::move(task));
        return future;
    }

    void ThreadPool::submitPackagedTask(std::packaged_task<void(void)> &&func) {
        if (!running) {
            STMS_WARN("Task submitted before ThreadPool was started! Please start the pool!");
            // Don't do anything.
        }

        {
            std::lock_guard<std::mutex> lg(unfinishedTaskMtx);
            unfinishedTasks++;
        }

        std::lock_guard<std::mutex> lg(this->taskQueueMtx);
        this->tasks.emplace(std::move(func));
        taskQueueCv.notify_one(); // should this be changed to notify_all?
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
            taskQueueCv.notify_all(); // Notify this worker that we requested it to stop

            this->workers.pop_back();
            if (this->workers.empty()) {
                STMS_WARN("The last thread was popped from ThreadPool! Stopping the pool!");
                this->running = false;
                taskQueueCv.notify_all(); // Notify all workers that we've stopped
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

        unsigned nThreads = 0;
        if (rhs.isRunning()) {
            STMS_WARN("ThreadPool moved while running! It will be restarted for the move!");
            nThreads = rhs.getNumThreads();
            rhs.waitIdle(1000);
            rhs.stop(true);
        }

        this->destroy();

        {
            // Hopefully locking ALL mutexes during the move is enough to prevent aforementioned race conditions.
            std::lock_guard<std::mutex> rhsWorkerLg(rhs.workerMtx);
            std::lock_guard<std::mutex> thisWorkerLg(this->workerMtx);
            std::lock_guard<std::mutex> rhsTaskLg(rhs.taskQueueMtx);
            std::lock_guard<std::mutex> thisTaskLg(this->taskQueueMtx);

            std::lock_guard<std::mutex> thisTaskCountLg(this->unfinishedTaskMtx);
            std::lock_guard<std::mutex> rhsTaskCountLg(rhs.unfinishedTaskMtx);

            // We cannot move the mutex so we quietly skip it and hope nobody notices. (Watch it crash and burn later)
            // Likewise, we cannot move the condition variables so we just quietly leave it be
            this->stopRequest = rhs.stopRequest;
            this->running = rhs.running.load();
            this->tasks = std::move(rhs.tasks);
            this->workers = std::move(rhs.workers);
            this->unfinishedTasks = rhs.unfinishedTasks;
        }

        if (nThreads > 0) {
            this->start(nThreads);
        }

        return *this;
    }

    ThreadPool::ThreadPool(ThreadPool &&rhs) noexcept {
        *this = std::move(rhs);
    }

    ThreadPool::~ThreadPool() {
        this->destroy();
    }

    void ThreadPool::waitIdle(unsigned timeout) {
        std::unique_lock<std::mutex> lg(unfinishedTaskMtx);
        if (unfinishedTasks == 0) {
            return;
        }

        auto predicate = [&]() { return unfinishedTasks == 0; };
        if (timeout == 0) {
            unfinishedTasksCv.wait(lg, predicate);
        } else {
            unfinishedTasksCv.wait_for(lg, std::chrono::milliseconds(timeout), predicate);
        }
    }

    TimedScheduler::TimedScheduler(PoolLike *parent) : pool(parent) {
        lastTick = std::chrono::steady_clock::now();
    }

    void TimedScheduler::tick() {
        auto now = std::chrono::steady_clock::now();

        for (auto &p : msIntervals) {
            p.second.time += std::chrono::duration_cast<std::chrono::nanoseconds>(now - lastTick).count() / 1000000.0f;
            // STMS_INFO("{} {}", p.second.time, p.second.interval);
            if (Cmp<float>::gt(p.second.time, p.second.interval)) {
                p.second.time -= p.second.interval;
                pool->submitTask(p.second.task);
            }
        }

        std::queue<TaskIdentifier> clearQueue;
        for (auto &p : msTimeouts) {
            float d = std::chrono::duration_cast<std::chrono::nanoseconds>(now - p.second.start).count() /  1000000.0f;
            if (Cmp<float>::gt(d, p.second.timeout)) {
                pool->submitPackagedTask(std::move(p.second.task)); // no catch for `std::packaged_task`s
                clearQueue.push(p.first);
            }
        }

        while (!clearQueue.empty()) {
            clearTimeout(clearQueue.front());
            clearQueue.pop();
        }

        lastTick = now;
    }

    TimeoutTask TimedScheduler::setTimeout(const std::function<void(void)> &task, float timeoutMs) {
        TaskIdentifier id = idAccumulator++;

        auto tm = MSTimeout{std::chrono::steady_clock::now(), timeoutMs, std::packaged_task<void(void)>(task)};
        auto ret = TimeoutTask{id, tm.task.get_future()};

        msTimeouts[id] = std::move(tm);
        return ret;
    }

    TaskIdentifier TimedScheduler::setInterval(const std::function<void(void)> &task, float intervalMs) {
        TaskIdentifier id = idAccumulator++;

        msIntervals[id] = MSInterval{0, intervalMs, task};

        return id;
    }

}
