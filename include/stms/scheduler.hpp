#pragma once

#ifndef __STONEMASON_SCHEDULER_HPP
#define __STONEMASON_SCHEDULER_HPP

#include <stms/async.hpp>
#include <stms/logging.hpp>
#include <stms/util/compare.hpp>
#include <iostream>

namespace stms {
    typedef uint64_t TaskIdentifier;

    struct TimeoutTask {
        TaskIdentifier id;
        std::future<void> future;
    };


    template <typename T>
    class Scheduler {
    protected:
        struct Interval {
            T time = 0;
            T interval;
            std::function<void(void)> task;
        };

        struct Timeout {
            T start;
            T timeout;
            std::packaged_task<void(void)> task;
        };

        PoolLike *pool = nullptr;
        T lastTick = 0;

        std::atomic_uint64_t idAccumulator = 0;

        std::unordered_map<TaskIdentifier, Timeout> timeouts;
        std::unordered_map<TaskIdentifier, Interval> intervals;


    public:
        Scheduler() = delete;

        explicit Scheduler(PoolLike *parent) : pool(parent) {};
        virtual ~Scheduler() = default;

        Scheduler &operator=(const Scheduler &rhs) = delete;
        Scheduler(const Scheduler &rhs) = delete;

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

        Scheduler(Scheduler &&rhs) {
            *this = std::move(rhs);
        }

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

        TimeoutTask setTimeout(const std::function<void(void)> &task, T timeoutTicks) {
            TaskIdentifier id = idAccumulator++;
            auto tm = Timeout{lastTick, timeoutTicks, std::packaged_task<void(void)>(task)};
            auto ret = TimeoutTask{id, tm.task.get_future()};
            timeouts[id] = std::move(tm);
            return ret;
        };

        inline void clearTimeout(TaskIdentifier id) {
            timeouts.erase(id);
        };

        TaskIdentifier setInterval(const std::function<void(void)> &task, T intervalTicks) {
            TaskIdentifier id = idAccumulator++;
            intervals[id] = {0, intervalTicks, task};
            return id;
        };

        inline void clearInterval(TaskIdentifier id) {
            intervals.erase(id);
        }

        inline PoolLike *&getPool() {
            return pool;
        }

        inline void setPool(PoolLike *p) {
            pool = p;
        }
    };

    class TimedScheduler {
    private:
        std::chrono::steady_clock::time_point lastTick;
    public:
        Scheduler<uint64_t> sched;

        explicit TimedScheduler(PoolLike *p);
        virtual ~TimedScheduler() = default;

        TimedScheduler &operator=(TimedScheduler &&rhs) {
            if (this == &rhs) { return *this; }
            lastTick = std::move(rhs.lastTick);
            sched = std::move(rhs.sched);
            return *this;
        }

        TimedScheduler(TimedScheduler &&rhs) : sched(nullptr) {
            *this = std::move(rhs);
        }

        inline void clearInterval(TaskIdentifier id) { sched.clearInterval(id); };
        inline void clearTimeout(TaskIdentifier id) { sched.clearTimeout(id); };

        inline TaskIdentifier setInterval(const std::function<void(void)> &task, float intervalMs) {
            return sched.setInterval(task, intervalMs * 1000000);
        }

        inline TimeoutTask setTimeout(const std::function<void(void)> &task, float timeoutMs) {
            return sched.setTimeout(task, timeoutMs * 1000000);
        }

        inline TaskIdentifier setIntervalNs(const std::function<void(void)> &task, uint64_t intervalNs) {
            return sched.setInterval(task, intervalNs);
        }

        inline TimeoutTask setTimeoutNs(const std::function<void(void)> &task, uint64_t timeoutNs) {
            return sched.setTimeout(task, timeoutNs);
        }

        void tick();

    };
}

#endif