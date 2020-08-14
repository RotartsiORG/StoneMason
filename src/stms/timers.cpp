//
// Created by grant on 5/3/20.
//

#include "stms/timers.hpp"

#include <thread>
#include "stms/logging.hpp"

// Needed for FLT_MAX
#include <cfloat>

#include "stms/except.hpp"

namespace stms {

    void Stopwatch::start() {
        startTime = std::chrono::steady_clock::now();
        state |= 3u; // Set the 0th (running) and 1st (start time set) bits
    }

    void Stopwatch::stop() {
        stopTime = std::chrono::steady_clock::now();
        state |= 4u; // Set the 2nd bit (stop time set)
        state &= ~1u; // Reset the 0th bit to FALSE (running)
    }

    float Stopwatch::getTime() {
        if ((state & 1u) && (state & 2u)) { // if running and start set
            return std::chrono::duration_cast<std::chrono::nanoseconds>(
                    std::chrono::steady_clock::now() - startTime).count() / 1000000.0f;
        } else if ((!(state & 1u)) && (state & 2u) && (state & 4u)) { // If not running & start set & stop set
            return std::chrono::duration_cast<std::chrono::nanoseconds>(stopTime - startTime).count() / 1000000.0f;
        }
        STMS_ERROR("Stopwatch::getTime() called when there is no time to get!");
        return 0;
    }

    void Stopwatch::reset() {
        if (!(state & 1u)) { // If it's not running
            STMS_WARN("Stopwatch::reset() called when Stopwatch was stopped!");
            if (exceptionLevel > 1) {
                throw std::runtime_error("Stopwatch::reset() called when Stopwatch was stopped!");
            }
            return;
        }

        startTime = std::chrono::steady_clock::now();
        state |= 2u; // set the 1st bit (start time set) to TRUE
        state &= ~(4u); // reset 2nd bit (stop time set) to FALSE
    }

    void TPSTimer::tick() {
        tickBefore = latestTick;
        latestTick = std::chrono::steady_clock::now();
    }

    float TPSTimer::getLatestMspt() {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(latestTick - tickBefore).count() / 1000000.0f;
    }

    float TPSTimer::getLatestTps() {
        float mspt = getLatestMspt();
        if (mspt > -0.001 && mspt < 0.001) {
            return FLT_MAX; // infinite tps
        } else {
            return 1000.0f / mspt;
        }
    }

    void TPSTimer::wait(float targetFps) {
        auto now = std::chrono::steady_clock::now();
        auto dur = now - latestTick;
        auto targetDur = std::chrono::nanoseconds(static_cast<long>(1000000000.0f / targetFps));
        if (targetDur > dur) {
            std::this_thread::sleep_for(targetDur - dur);
        }
    }
}
