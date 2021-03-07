/**
 * @file stms/async.hpp
 * @brief `ThreadPool`s for async task scheduling and execution.
 * Created by grant on 12/30/19.
 */

#pragma once

#ifndef __STONEMASON_ASYNC_HPP
#define __STONEMASON_ASYNC_HPP
//!< Include guard

#include <queue>
#include <cinttypes>
#include <future>
#include <chrono>
#include <unordered_map>
#include "stms/config.hpp"

namespace stms {

    class PoolLike {
    public:
        // undefined constructor
        virtual ~PoolLike() = default;

        virtual std::future<void> submitTask(const std::function<void(void)> &func) = 0;
        virtual void submitPackagedTask(std::packaged_task<void(void)> &&func) = 0;


        virtual void start(unsigned threads = 0) = 0;
        virtual void stop(bool block = true) = 0;
        virtual bool isRunning() const = 0;

        virtual void waitIdle(unsigned timeout = 0) = 0;

        // virtual void pushThread() = 0;
        // virtual void popThread(bool block = true) = 0;
    };

    /**
     * @brief Pretend to be a thread pool, but execute tasks instantly inside of the submit function 
     */
    class InstaPool : public PoolLike {
    public:
        ~InstaPool() override = default;

        std::future<void> submitTask(const std::function<void(void)> &func) override;
        void submitPackagedTask(std::packaged_task<void(void)> &&func) override;

        bool isRunning() const override { return true; }

        void start(unsigned = 0) override {};
        void stop(bool = true) override {};
        void waitIdle(unsigned = 0) override {};
    };

    inline PoolLike *getDefaultInstaPool() {
        static InstaPool pool{};
        return &pool;
    }

    class ThreadPool;

    static void workerFunc(ThreadPool *parent, size_t index); //!< Internal implementation detail. Don't touch.

    // TODO: Pool-like with a dummy pool?

    /**
     * @brief A thread pool. Submitted tasks will be automagically executed by a thread in the pool.
     */
    class ThreadPool : public PoolLike {
    private:
        std::mutex taskQueueMtx; //!< Mutex to lock for accessing `tasks`. Internal implementation detail.
        std::mutex workerMtx; //!< Mutex to lock for accessing `workers`. Internal implementation detail.
        std::mutex unfinishedTaskMtx; //!< Mutex to lock for accessing `unfinishedTasks`. Internal impl detail.

        std::condition_variable taskQueueCv; //!< Condition variable for blocking workers until there are jobs.
        std::condition_variable unfinishedTasksCv; //!< Condition variable used for blocking in `waitIdle()`.

        unsigned short unfinishedTasks = 0; //!< Number of tasks that are incomplete.
        std::queue<std::packaged_task<void (void)>> tasks; //!< Queue of tasks to execute
        std::deque<std::thread> workers; //!< A list of worker threads.

        std::atomic_bool running = false; //!< True if the thread pool is running. (Duh)
        size_t stopRequest = 0; //!< The thread ID that we request to stop.

        friend void workerFunc(ThreadPool *parent, size_t index); //!< Static worker function. Internal impl detail.

        void destroy(); //!< Destroy the thread pool. The functionality of the destructor needs to be invoked elsewhere.

    public:
        /// Deleted copy constructor
        ThreadPool &operator=(const ThreadPool &rhs) = delete;

        /// Deleted copy assignment operator.
        ThreadPool(const ThreadPool &rhs) = delete;

        /**
         * @brief Move copy operator=
         * @param rhs Right Hand Side of the `std::move`
         * @return A reference to this instance.
         */
        ThreadPool &operator=(ThreadPool &&rhs) noexcept;

        /**
         * @brief Move copy constructor
         * @param rhs Right Hand Side of `std::move`
         */
        ThreadPool(ThreadPool &&rhs) noexcept;

        ThreadPool() = default; //!< default constructor

        ~ThreadPool() override; //!< Virtual destructor

        /**
         * @brief Start the thread pool, or if the pool is already running, restart it with the new number of threads.
         * @param threads Number of threads to create for the pool. If it is 0, then we default to
         *                `std::thread::hardware_concurrency()`. If that is still 0, we default to 8.
         */
        void start(unsigned threads = 0) override;

        /**
         * @brief Stop the thread pool. Any newly submitted tasks will NOT be executed!
         * @param block If true, this will block until all the worker threads have finished their current task.
         *              Otherwise, they are detached to finish by themselves.
         */
        void stop(bool block = true) override;

        /**
         * @brief Submit a function to the thread pool for execution (if the `ThreadPool` is started).
         * @param func Function to execute
         * @return A void future that you can wait on to block until the task is finished.
         */
        std::future<void> submitTask(const std::function<void(void)> &func) override;

        void submitPackagedTask(std::packaged_task<void(void)> &&func) override;

        void pushThread(); //!< Add 1 worker thread to the thread pool

        /**
         * @brief Remove 1 worker thread from the pool
         * @param block If true, we block until this worker thread finishes executing its current task.
         *              Otherwise, we just detach it to finish up by itself.
         */
        void popThread(bool block = true);

        /**
         * @brief Get the number of worker threads still active
         * @return Number of threads.
         */
        inline size_t getNumThreads() {
            std::lock_guard<std::mutex> lg(this->workerMtx);
            return workers.size();
        }

        /**
         * @brief Query the number of unfinished tasks
         * @return The number of tasks awaiting execution
         */
        inline size_t getNumTasks() {
            std::lock_guard<std::mutex> lg(this->unfinishedTaskMtx);
            return unfinishedTasks;
        }

        /**
         * @brief Block until all tasks in the thread pool are finished (aka the thread pool is *idle*)
         * @param timeout Maximum number of milliseconds to block for. If set to 0, this will block infinitely
         */
        void waitIdle(unsigned timeout = 0) override;

        /**
         * @brief Query if the thread pool is still running
         * @return True if running.
         */
        [[nodiscard]] bool isRunning() const override {
            return running;
        }
    };


    typedef uint64_t TaskIdentifier;

    struct TimeoutTask {
        TaskIdentifier id;
        std::future<void> future;
    };

    struct IntervalSettings {
        bool waitForCompletionBeforeReschedule = false;
        bool executeImmediately = false;
    };

    class TimedScheduler {
    private:
        struct MSInterval {
            float time;
            float interval;
            std::function<void(void)> task;
        };

        struct MSTimeout {
            std::chrono::steady_clock::time_point start;
            float timeout;
            std::packaged_task<void(void)> task;
        };

        PoolLike *pool = nullptr;
        std::chrono::steady_clock::time_point lastTick;

        std::atomic_uint64_t idAccumulator = 0;

        std::unordered_map<TaskIdentifier, MSTimeout> msTimeouts;
        std::unordered_map<TaskIdentifier, MSInterval> msIntervals;


    public:
        explicit TimedScheduler(PoolLike *parent);
        virtual ~TimedScheduler() = default;

        void tick();

        TimeoutTask setTimeout(const std::function<void(void)> &task, float timeoutMs);

        inline void clearTimeout(TaskIdentifier id) {
            msTimeouts.erase(id);
        };

        TaskIdentifier setInterval(const std::function<void(void)> &task, float intervalMs);

        inline void clearInterval(TaskIdentifier id) {
            msIntervals.erase(id);
        }

        inline PoolLike *&getPool() {
            return pool;
        }

        inline void setPool(PoolLike *p) {
            pool = p;
        }
    };
}

#endif //__STONEMASON_ASYNC_HPP
