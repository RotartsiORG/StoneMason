/**
 * @file stms/timers.hpp
 * @brief Provides timers `Stopwatch` and `TPSTimer`.
 * Created by grant on 5/3/20.
 */

#pragma once

#ifndef __STONEMASON_TIMERS_HPP
#define __STONEMASON_TIMERS_HPP
//!< Include guard

#include <chrono>
#include "stms.hpp"

namespace stms {
    /**
     * @brief Stopwatch for timing. Can tell you how long it has been since `start()`,
     *        or it can tell you the time between `start()` and `stop()`.
     */
    class Stopwatch {
    private:
        std::chrono::steady_clock::time_point startTime; //!< Time point when `start()` is called.
        std::chrono::steady_clock::time_point stopTime; //!< Time point when `stop()` is called

        /**
         * @brief State of the stopwatch. Internal implementation detail. Don't touch.
         *        Bit 0 is a bool representing if the stopwatch is running.
         *        Bit 1 is a bool representing if `startTime` has been set.
         *        Bit 2 is a bool representing if `stopTime` has been set.
         */
        uint8_t state{};

    public:

        Stopwatch() = default; //!< default constructor

        virtual ~Stopwatch() = default; //!< Default virtual destructor

        /**
         * @brief Default copy operator=
         * @param rhs Right hand side to copy
         * @return Reference to this instance.
         */
        Stopwatch &operator=(const Stopwatch &rhs) noexcept = default;

        /**
         * @brief Default copy constructor
         * @param rhs Right Hand Side to copy
         */
        Stopwatch(const Stopwatch &rhs) noexcept = default;

        void start(); //!< Start the stopwatch

        void stop(); //!< stop the stopwatch

        /**
         * @brief If `stop()` hasn't been called, get the time since the call to `start()`, otherwise get the
         *        time between `start()` and `stop()`
         * @return Time in milliseconds, 0 if there's no time to get.
         */
        float getTime();

        void reset(); //!< Reset the stopwatch. Forget the start/stop times.

        /**
         * @brief Query if the stopwatch is running (i.e. `start()` called, but not `stop()`.)
         * @return True if running, false otherwise.
         */
        [[nodiscard]] inline bool isRunning() const {
            return getBit(state, 0);
        }
    };

    /**
     * @brief Timer for measuring TPS (ticks per second) or MSPT (milliseconds per tick)
     */
    class TPSTimer {
    private:
        std::chrono::steady_clock::time_point tickBefore; //!< Time point of the 2nd last `tick()` call.
        std::chrono::steady_clock::time_point latestTick; //!< Time point of the last `tick()` call.
    public:
        TPSTimer() = default; //!< default constructor

        virtual ~TPSTimer() = default; //!< default virtual destructor

        /**
         * @brief Default copy operator=
         * @param rhs Right Hand Side to be copied
         * @return Reference to this instance
         */
        TPSTimer &operator=(const TPSTimer &rhs) noexcept = default;

        /**
         * @brief default copy constructor
         * @param rhs Right Hand Side to be copied.
         */
        TPSTimer(const TPSTimer &rhs) noexcept = default;

        void tick(); //!< Register a tick

        /**
         * @brief If we are ticking faster than the `targetFps`, block until we are at `targetFps`.
         * @param targetFps The number of ticks per second we are aiming for.
         */
        void wait(float targetFps = 60);

        /**
         * @brief Get the amount of time the last tick took
         * @return Amount of time the last tick took, in milliseconds
         */
        float getLatestMspt();

        /**
         * @brief Get the Ticks Per Second, calculated using the last 2 ticks.
         * @return TPS in milliseconds
         */
        float getLatestTps();
    };

}


#endif //__STONEMASON_TIMERS_HPP
