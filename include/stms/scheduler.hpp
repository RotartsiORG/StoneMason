/**
 * @file stms/scheduler.hpp
 * @author Grant Yang (rotartsi0482@gmail.com)
 * @brief Schedulers providing functionality imitating JavaScript's `setInterval` and `setTimeout`, with both
 *        manually ticked and automatically timed versions.
 * @date 2021-03-28
 */
#pragma once

#ifndef __STONEMASON_SCHEDULER_HPP
#define __STONEMASON_SCHEDULER_HPP
//!< Include guard.

#include <stms/async.hpp>
#include <stms/logging.hpp>
#include <stms/util/compare.hpp>
#include <iostream>

namespace stms {
    /// Handle type for timeouts and intervals within a single `Scheduler` object. `uint64_t`.
    typedef uint64_t TaskIdentifier;

    /// Info for timeout submitted to scheduler
    struct TimeoutTask {
        TaskIdentifier id; //!< Identification used to clear the timeout.
        std::future<void> future; //!< Future for timeout. Used to block until completion or query for exceptions.
    };


    /**
     * @brief Schedule tasks to execute after a timeout or at an interval (once every x ticks)
     * @tparam T Type of internal tick counter used to determine if a task should execute.
     */
    template <typename T>
    class Scheduler {
    protected:

        /// Internal implementation detail & data for intervals.
        struct Interval {
            T time = 0; //!< Ticks that have passed since last execution
            T interval; //!< How often the interval is scheduled to run in ticks
            std::function<void(void)> task; //!< Function to execute. **WARNING: EXCEPTIONS ARE SWALLOWED**
        };

        /// Internal implementation detail & data for timeouts
        struct Timeout {
            T start; //!< Tick number which this Timeout was started on
            T timeout; //!< Length of the timeout. (Number of ticks to wait for before execution)
            std::packaged_task<void(void)> task; //!< Task to execute.
        };

        PoolLike *pool = nullptr; //!< Pool to submit tasks to.
        T lastTick = 0; //!< Tick counter.

        std::atomic<TaskIdentifier> idAccumulator = 0; //!< Counter for unique IDs for every interval/timeout.

        std::unordered_map<TaskIdentifier, Timeout> timeouts; //!< Map of pending timeouts
        std::unordered_map<TaskIdentifier, Interval> intervals; //!< Map of active intervals.

    public:
        /**
         * @brief Deleted default constructor.
         * Schedulers MUST be initialized with a PoolLike.
         */
        Scheduler() = delete;

        /**
         * @brief Construct a new Scheduler object
         * @param parent Pool to submit tasks to.
         */
        explicit Scheduler(PoolLike *parent) : pool(parent) {};
        virtual ~Scheduler() = default; //!< Default destructor

        Scheduler &operator=(const Scheduler &rhs) = delete; //!< Deleted copy assignment operator (Due to packaged_task)
        Scheduler(const Scheduler &rhs) = delete; //!< Deleted copy constructor (Due to packaged_task)

        /// Move assignment operator.
        Scheduler &operator=(Scheduler &&rhs) {
            if (this == &rhs) {
                return *this;
            }

            pool = rhs.pool;
            lastTick = rhs.lastTick;
            idAccumulator = rhs.idAccumulator.load();
            timeouts = std::move(rhs.timeouts);
            intervals = std::move(rhs.intervals);
            return *this;
        }

        /// Move assignment constructor.
        Scheduler(Scheduler &&rhs) {
            *this = std::move(rhs);
        }

        /**
         * @brief Update the scheduler and execute tasks due for execution.
         * @param inc Number of ticks to advance by
         * @return T Total number of ticks that have passed.
         */
        T tick(T inc = 0) {
            lastTick += inc;
            // STMS_DEBUG("{}", lastTick);

            for (auto &i : intervals) {
                i.second.time += inc;
                if (i.second.time >= i.second.interval) {
                    i.second.time -= i.second.interval;
                    pool->submitTask(i.second.task);
                }
            }

            std::queue<TaskIdentifier> clearQueue;
            for (auto &t : timeouts) {
                if ((lastTick - t.second.start) >= t.second.timeout) {
                    pool->submitPackagedTask(std::move(t.second.task));
                    clearQueue.push(t.first);
                }
            }
 
            while (!clearQueue.empty()) {
                clearTimeout(clearQueue.front());
                clearQueue.pop();
            }

            return lastTick;
        };

        /**
         * @brief Schedule a timeout for execution. Returns immediately.
         * @param task Function to execute
         * @param timeoutTicks Number of ticks to wait before execution
         * @return TimeoutTask Information about the timeout.
         */
        TimeoutTask setTimeout(const std::function<void(void)> &task, T timeoutTicks) {
            TaskIdentifier id = idAccumulator++;
            auto tm = Timeout{lastTick, timeoutTicks, std::packaged_task<void(void)>(task)};
            auto ret = TimeoutTask{id, tm.task.get_future()};
            timeouts[id] = std::move(tm);
            return ret;
        };

        /**
         * @brief Cancel a timeout and keep it from being executed.
         * @param id Timeout to cancel. Value returned from `setTimeout`
         */
        inline void clearTimeout(TaskIdentifier id) {
            timeouts.erase(id);
        };

        /**
         * @brief Schedule an interval for execution. Returns immediately.
         * @param task Function to execute.
         * @param intervalTicks How often the interval should execute
         * @return TaskIdentifier ID handle for this interval.
         */
        TaskIdentifier setInterval(const std::function<void(void)> &task, T intervalTicks) {
            TaskIdentifier id = idAccumulator++;
            intervals[id] = {0, intervalTicks, task};
            return id;
        };

        /**
         * @brief Cancel an interval and keep it from being executed again.
         * @param id Interval to cancel. Value returned from `setInterval`
         */
        inline void clearInterval(TaskIdentifier id) {
            intervals.erase(id);
        }

        /**
         * @brief Get the `pool` object
         * @return PoolLike*& Pool object tasks are submitted to. This reference is mutable.
         */
        inline PoolLike *&getPool() {
            return pool;
        }

        /**
         * @brief Set the `pool` object
         * @param p New pool to submit tasks to.
         */
        inline void setPool(PoolLike *p) {
            pool = p;
        }
    };

    /**
     * @warning This scheduler will overflow in ~584.6 years so don't leave this running that long. 
     *          Also, don't expect this to be very accurate, as `tick()` may drift it out of sync ~0.5ns every call.
     * @brief Schedule tasks to execute after a timeout or at an interval (once every x milliseconds).
     *        Effectively version of `Scheduler<T>` but using the actual time instead of manually ticking.
     */
    class TimedScheduler {
    private:
        std::chrono::steady_clock::time_point lastTick; //!< Last time tick() was called. Internal implementation detail.
    public:
        Scheduler<uint64_t> sched; //!< Internal scheduler used. 1 tick = 1 nanosecond.

        /**
         * @brief Construct a new Timed Scheduler object
         * @param p Pool to submit tasks to
         */
        explicit TimedScheduler(PoolLike *p);
        virtual ~TimedScheduler() = default; //!< Default virtual destructor

        TimedScheduler &operator=(TimedScheduler &&rhs); //!< Move assignment operator
        TimedScheduler(TimedScheduler &&rhs); //!< Move constructor

        TimedScheduler &operator=(const TimedScheduler &rhs) = delete; //!< Deleted copy assignment operator
        TimedScheduler(const TimedScheduler &rhs) = delete; //!< Deleted copy constructor

        /**
         * @brief Cancel an interval and keep it from executing. Forwards to `Scheduler<T>::clearInterval`.
         * @param id Interval to cancel.
         */
        inline void clearInterval(TaskIdentifier id) { sched.clearInterval(id); };

        /**
         * @brief Cancel a timeout and keep it from executing. Forwards to `Scheduler<T>::clearTimeout`.
         * @param id Timeout to cancel.
         */
        inline void clearTimeout(TaskIdentifier id) { sched.clearTimeout(id); };

        /**
         * @brief Submit an interval to execute once every `intervalMs` milliseconds. Returns immediately.
         * @see `Scheduler<T>::setInterval`
         * @param task Task to execute
         * @param intervalMs How often to execute it
         * @return TaskIdentifier ID handler for interval.
         */
        inline TaskIdentifier setInterval(const std::function<void(void)> &task, float intervalMs) {
            return sched.setInterval(task, intervalMs * 1000000);
        }

        /**
         * @brief Submit a timeout to execute after `intervalMs` milliseconds. Returns immediately.
         * @see `Scheduler<T>::setTimeout`
         * @param task Task to execute
         * @param intervalMs How often to execute it
         * @return TaskIdentifier ID handler for interval.
         */
        inline TimeoutTask setTimeout(const std::function<void(void)> &task, float timeoutMs) {
            return sched.setTimeout(task, timeoutMs * 1000000);
        }


        /**
         * @brief Submit an interval to execute once every `intervalNs` nanoseconds. Returns immediately.
         * @see `Scheduler<T>::setInterval`
         * @param task Task to execute
         * @param intervalNs How often to execute it
         * @return TaskIdentifier ID handler for interval.
         */
        inline TaskIdentifier setIntervalNs(const std::function<void(void)> &task, uint64_t intervalNs) {
            return sched.setInterval(task, intervalNs);
        }

        /**
         * @brief Submit a timeout to execute after `intervalNs` nanoseconds. Returns immediately.
         * @see `Scheduler<T>::setTimeout`
         * @param task Task to execute
         * @param intervalNs How often to execute it
         * @return TaskIdentifier ID handler for interval.
         */
        inline TimeoutTask setTimeoutNs(const std::function<void(void)> &task, uint64_t timeoutNs) {
            return sched.setTimeout(task, timeoutNs);
        }

        /**
         * @brief Execute any timeouts/intervals due for execution. May drift out of sync by ~0.5ns every call.
         */
        void tick();

    };
}

#endif