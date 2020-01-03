//
// Created by grant on 12/30/19.
//

#pragma once

#ifndef __STONEMASON_ASYNC_HPP
#define __STONEMASON_ASYNC_HPP

#include <queue>
#include <cinttypes>
#include <future>

namespace stms {
    class ThreadPool;

    static void workerFunc(ThreadPool *parent, size_t index);

    class ThreadPool {
    private:
        struct ThreadPoolTask {
        public:
            uint32_t priority = 8;
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

        void
        destroy(); // This function is needed since the destructor does unwanted dumb shit when we call it from operator=

    public:
        // The number of milliseconds the workers should sleep for before checking for a task. If set too low,
        // worker threads may throttle the CPU. If set too high, then the workers would waste time idling.
        uint32_t workerDelay = 125;

        // Deleted copy constructor and copy assignment operator.
        ThreadPool &operator=(const ThreadPool &rhs) = delete;

        ThreadPool(const ThreadPool &rhs) = delete;

        ThreadPool &operator=(ThreadPool &&rhs) noexcept;

        ThreadPool(ThreadPool &&rhs) noexcept;

        ThreadPool() = default;

        ~ThreadPool();

        // If `threads` is 0, then the ret value of `std::thread::hardware_concurrency()` is used.
        void start(uint32_t threads = 0);

        // This function will block until all ongoing tasks are complete.
        // No new tasks waiting in queue would be accepted.
        void stop();

        std::future<void *> submitTask(std::function<void *(void *)> func, void *dat, uint32_t priority);

        void pushWorker();

        void popWorker();

        inline size_t getNumWorkers() {
            std::lock_guard<std::recursive_mutex> lg(this->workerMtx);
            return workers.size();
        }

        inline size_t getNumTasks() {
            std::lock_guard<std::recursive_mutex> lg(this->taskQueueMtx);
            return tasks.size();
        }

        void waitIdle();

        inline bool isRunning() {
            return running;
        }
    };
}

#endif //__STONEMASON_ASYNC_HPP
