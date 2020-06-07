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
#include "stms/logging.hpp"

namespace stms {
    class ThreadPool;

    static void workerFunc(ThreadPool *parent, size_t index);

    class ThreadPool {
    private:
        struct ThreadPoolTask {
        public:
            unsigned priority = 8;
            std::packaged_task<void *(void *)> task;
            void *pData{};

            ThreadPoolTask() = default;

            ThreadPoolTask(ThreadPoolTask &&rhs) noexcept;

            ThreadPoolTask &operator=(ThreadPoolTask &&rhs) noexcept;
        };

        class TaskComparator {
        private:
            bool reverse;
        public:
            explicit TaskComparator(const bool &rev = false);

            bool operator()(const ThreadPoolTask &lhs, const ThreadPoolTask &rhs) const;
        };

        std::recursive_mutex taskQueueMtx;
        std::recursive_mutex workerMtx;

        std::priority_queue<ThreadPoolTask, std::vector<ThreadPoolTask>, TaskComparator> tasks
                = std::priority_queue<ThreadPoolTask, std::vector<ThreadPoolTask>, TaskComparator>();
        std::deque<std::thread> workers;

        bool running = false;
        size_t stopRequest = 0;

        friend void workerFunc(ThreadPool *parent, size_t index);

        inline void destroy() {
            if (this->running) {
                STMS_PUSH_WARNING("ThreadPool destroyed while running! Stopping it now (with block=true)");
                stop(true);
            }

            if (!tasks.empty()) {
                STMS_PUSH_WARNING("ThreadPool destroyed with unfinished tasks! {} tasks will never be executed!", tasks.size());
            }
        }

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

        std::future<void *> submitTask(std::function<void *(void *)> func, void *dat, unsigned priority);

        void pushThread();

        // If block is true, then we block until the thread finishes executing, otherwise, it is detached.
        void popThread(bool block = true);

        inline size_t getNumThreads() {
            std::lock_guard<std::recursive_mutex> lg(this->workerMtx);
            return workers.size();
        }

        inline size_t getNumTasks() {
            std::lock_guard<std::recursive_mutex> lg(this->taskQueueMtx);
            return tasks.size();
        }

        void waitIdle();

        [[nodiscard]] inline bool isRunning() const {
            return running;
        }
    };
}

#endif //__STONEMASON_ASYNC_HPP
