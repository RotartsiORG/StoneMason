//
// Created by grant on 5/3/20.
//

#pragma once

#ifndef __STONEMASON_TIMERS_HPP
#define __STONEMASON_TIMERS_HPP

#include <chrono>
#include "stms.hpp"

namespace stms {
    class Stopwatch {
    private:
        std::chrono::steady_clock::time_point startTime;
        std::chrono::steady_clock::time_point stopTime;

        // Bit 0 is running
        // Bit 1 is startDefined
        // Bit 2 is endDefined
        uint8_t state{};

    public:

        Stopwatch() = default;

        virtual ~Stopwatch() = default;

        Stopwatch &operator=(const Stopwatch &rhs) noexcept = default;

        Stopwatch(const Stopwatch &rhs) noexcept = default;

        void start();

        void stop();

        float getTime();

        void reset();

        [[nodiscard]] inline bool isRunning() const {
            return getBit(state, 0);
        }
    };

    class TPSTimer {
    private:
        std::chrono::steady_clock::time_point tickBefore;
        std::chrono::steady_clock::time_point latestTick;
    public:
        TPSTimer() = default;

        virtual ~TPSTimer() = default;

        TPSTimer &operator=(const TPSTimer &rhs) noexcept = default;

        TPSTimer(const TPSTimer &rhs) noexcept = default;

        void tick();

        void wait(float targetFps = 60);

        float getLatestMspt();

        float getLatestTps();
    };

}


#endif //__STONEMASON_TIMERS_HPP
