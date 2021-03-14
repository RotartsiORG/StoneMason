#include "stms/scheduler.hpp"

namespace stms {
    TimedScheduler::TimedScheduler(PoolLike *p) : sched{p} {
        lastTick = std::chrono::steady_clock::now();
    };

    void TimedScheduler::tick() {
        auto now = std::chrono::steady_clock::now();
        int64_t ns = std::chrono::duration_cast<std::chrono::nanoseconds>(now - lastTick).count();
        sched.tick(ns);
        lastTick = now;
    }
}