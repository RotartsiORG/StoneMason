//
// Created by grant on 12/30/19.
//

#pragma once

#ifndef __STONEMASON_ASYNC_HPP
#define __STONEMASON_ASYNC_HPP

#include <queue>
#include <cinttypes>
#include <future>
#include "stms/config.hpp"

namespace stms {
    class ThreadPool;

    static void workerFunc(ThreadPool *parent, size_t index);

    class ThreadPool {
    private:
        std::mutex taskQueueMtx;
        std::mutex workerMtx;
        std::mutex unfinishedTaskMtx;

        unsigned short unfinishedTasks;
        std::queue<std::packaged_task<void (void)>> tasks;
        std::deque<std::thread> workers;

        bool running = false;
        size_t stopRequest = 0;

        friend void workerFunc(ThreadPool *parent, size_t index);

        void destroy();

    public:
        // The number of milliseconds the workers should sleep for before checking for a task. If set too low,
        // worker threads may throttle the CPU. If set too high, then the workers would waste time idling.
        unsigned workerDelay = threadPoolWorkerDelayMs;

        // Deleted copy constructor and copy assignment operator.
        ThreadPool &operator=(const ThreadPool &rhs) = delete;

        ThreadPool(const ThreadPool &rhs) = delete;

        ThreadPool &operator=(ThreadPool &&rhs) noexcept;

        ThreadPool(ThreadPool &&rhs) noexcept;

        ThreadPool() = default;

        virtual ~ThreadPool();

        // If `threads` is 0, then the ret value of `std::thread::hardware_concurrency()` is used.
        void start(unsigned threads = 0);

        // This function will block until all ongoing tasks are complete if block is set to true, otherwise
        // all worker threads will be detached to complete by themselves.
        // No new tasks waiting in queue would be accepted.
        void stop(bool block = true);

        /**
         * Submit a function to the thread pool for execution (if the `ThreadPool` is started).
         * @param func Function to execute
         * @return A void future that you can wait on to block until the task is finished.
         *         **NOTE: When C++20 is finalized, this may or may not get replaced by a `std::binary_semaphore`.**
         */
        std::future<void> submitTask(const std::function<void(void)> &func);

        void pushThread();

        // If block is true, then we block until the thread finishes executing, otherwise, it is detached.
        void popThread(bool block = true);

        inline size_t getNumThreads() {
            std::lock_guard<std::mutex> lg(this->workerMtx);
            return workers.size();
        }

        inline size_t getNumTasks() {
            std::lock_guard<std::mutex> lg(this->taskQueueMtx);
            return tasks.size();
        }

        void waitIdle();

        [[nodiscard]] inline bool isRunning() const {
            return running;
        }
    };
}

#endif //__STONEMASON_ASYNC_HPP
