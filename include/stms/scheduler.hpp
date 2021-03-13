#pragma once

#ifndef __STONEMASON_SCHEDULER_HPP
#define __STONEMASON_SCHEDULER_HPP

#include <stms/async.hpp>
#include <stms/util/compare.hpp>

namespace stms {
    typedef uint64_t TaskIdentifier;

    struct TimeoutTask {
        TaskIdentifier id;
        std::future<void> future;
    };

    template <typename InType, typename TickType>
    class _stms_Scheduler {
    protected:
        struct Interval {
            InType time = 0;
            InType interval;
            std::function<void(void)> task;
        };

        struct Timeout {
            TickType start;
            InType timeout;
            std::packaged_task<void(void)> task;
        };

        PoolLike *pool = nullptr;
        TickType lastTick;

        std::atomic_uint64_t idAccumulator = 0;

        std::unordered_map<TaskIdentifier, Timeout> timeouts;
        std::unordered_map<TaskIdentifier, Interval> intervals;


    public:
        _stms_Scheduler() = delete;

        explicit _stms_Scheduler(PoolLike *parent);
        virtual ~_stms_Scheduler() = default;

        TickType tick(InType inc = 0);

        TimeoutTask setTimeout(const std::function<void(void)> &task, InType timeoutMs);

        inline void clearTimeout(TaskIdentifier id) {
            timeouts.erase(id);
        };

        TaskIdentifier setInterval(const std::function<void(void)> &task, InType intervalMs);

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

    typedef _stms_Scheduler<int64_t, int64_t> TickedScheduler;

    template<>
    TickedScheduler::_stms_Scheduler(PoolLike *parent) : pool(parent) {};

    template<>
    int64_t TickedScheduler::tick(int64_t inc) {
        lastTick += inc;

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
    }

    template<>
    TaskIdentifier TickedScheduler::setInterval(const std::function<void(void)> &task, int64_t intervalTicks) {
        TaskIdentifier id = idAccumulator++;
        intervals[id] = {0, intervalTicks, task};
        return id;
    }

    template<>
    TimeoutTask TickedScheduler::setTimeout(const std::function<void(void)> &task, int64_t timeoutTicks) {
        TaskIdentifier id = idAccumulator++;
        auto tm = Timeout{lastTick, timeoutTicks, std::packaged_task<void(void)>(task)};
        auto ret = TimeoutTask{id, tm.task.get_future()};
        timeouts[id] = std::move(tm);
        return ret;
    }


    typedef _stms_Scheduler<float, std::chrono::steady_clock::time_point> TimedScheduler;

    template<>
    TimedScheduler::_stms_Scheduler(PoolLike *parent) : pool(parent) {
        lastTick = std::chrono::steady_clock::now();
    }

    template<>
    std::chrono::steady_clock::time_point TimedScheduler::tick(float inc) {
        auto now = std::chrono::steady_clock::now();

        for (auto &p : intervals) {
            p.second.time += std::chrono::duration_cast<std::chrono::nanoseconds>(now - lastTick).count() / 1000000.0f;
            // STMS_INFO("{} {}", p.second.time, p.second.interval);
            if (Cmp<float>::gt(p.second.time, p.second.interval)) {
                p.second.time -= p.second.interval;
                pool->submitTask(p.second.task);
            }
        }

        std::queue<TaskIdentifier> clearQueue;
        for (auto &p : timeouts) {
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
        return now;
    }

    template<>
    TimeoutTask TimedScheduler::setTimeout(const std::function<void(void)> &task, float timeoutMs) {
        TaskIdentifier id = idAccumulator++;

        auto tm = Timeout{std::chrono::steady_clock::now(), timeoutMs, std::packaged_task<void(void)>(task)};
        auto ret = TimeoutTask{id, tm.task.get_future()};

        timeouts[id] = std::move(tm);
        return ret;
    }

    template<>
    TaskIdentifier TimedScheduler::setInterval(const std::function<void(void)> &task, float intervalMs) {
        TaskIdentifier id = idAccumulator++;
        intervals[id] = Interval{0, intervalMs, task};
        return id;
    }
}

#endif