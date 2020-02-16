//
// Created by grant on 12/30/19.
//

#pragma once

#ifndef __STONEMASON_ASYNC_HPP
#define __STONEMASON_ASYNC_HPP

#include <queue>
#include <cinttypes>
#include <future>
#include "config.hpp"

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

        void
        destroy(); // This function is needed since the destructor does unwanted dumb shit when we call it from operator=

        void rawSubmit(std::packaged_task<void *(void *)> *task, void *data, unsigned priority);

        friend class IOService;

    public:
        // The number of milliseconds the workers should sleep for before checking for a task. If set too low,
        // worker threads may throttle the CPU. If set too high, then the workers would waste time idling.
        unsigned workerDelay = STMS_DEFAULT_WORKER_DELAY;

        // Deleted copy constructor and copy assignment operator.
        ThreadPool &operator=(const ThreadPool &rhs) = delete;

        ThreadPool(const ThreadPool &rhs) = delete;

        ThreadPool &operator=(ThreadPool &&rhs) noexcept;

        ThreadPool(ThreadPool &&rhs) noexcept;

        ThreadPool() = default;

        virtual ~ThreadPool();

        // If `threads` is 0, then the ret value of `std::thread::hardware_concurrency()` is used.
        void start(unsigned threads = 0);

        // This function will block until all ongoing tasks are complete.
        // No new tasks waiting in queue would be accepted.
        void stop();

        std::future<void *> submitTask(std::function<void *(void *)> func, void *dat, unsigned priority);

        void pushThread();

        void popThread();

        inline size_t getNumThreads() {
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

    class IOService {
    private:
        struct PoolAndRange {
            ThreadPool *threadPool = nullptr;
            unsigned id = 0;

            // Ranges are inclusive
            unsigned minPriority = 0;
            unsigned maxPriority = -1;
        };

        struct Timeout {
            std::packaged_task<void *(void *)> task;
            unsigned id = 0;
            void *data{};
            std::chrono::time_point<std::chrono::steady_clock, std::chrono::nanoseconds> start;
            unsigned delay;
            unsigned priority;
        };

        struct Interval {
            std::function<void(void *)> func;
            unsigned id = 0;
            void *data{};
            bool timeExec;
        };

        std::vector<PoolAndRange> pools;
        unsigned nextPoolID = 0;

        std::vector<Interval> intervals;
        unsigned nextIntervalID = 0;

        std::vector<Timeout> timeouts;
        unsigned nextTiemoutID = 0;

        void rawSubmit(std::packaged_task<void *(void *)> *task, void *data, unsigned priority);

    public:
        IOService();

        virtual ~IOService();

        // Returns pool ID to be used in `removePool`
        unsigned addPool(ThreadPool *pool, unsigned min = 0, unsigned max = -1);

        void removePool(unsigned pool);

        std::future<void *> submitTask(std::function<void *(void *)> func, void *dat, unsigned priority);

        // Should the execution time count towards the cooldown?
        unsigned setInterval(const std::function<void(void *)> &func, void *data, bool timeExecution);

        void clearInterval(unsigned interval);

        unsigned setSubmitInterval(const std::function<void *(void *)> &func, void *data);

        void clearSubmitInterval(unsigned interval);

        // Delay in milliseconds
        std::future<void *>
        setTimeout(std::function<void *(void *)> func, void *data, unsigned delay, unsigned priority,
                   unsigned *timeoutID = nullptr);

        void clearTimeout(unsigned timeout);

        void update();

        IOService &operator=(IOService &rhs) = delete;

        IOService(IOService &rhs) = delete;

        IOService &operator=(IOService &&rhs) noexcept;

        IOService(IOService &&rhs) noexcept;
    };
}

#endif //__STONEMASON_ASYNC_HPP
