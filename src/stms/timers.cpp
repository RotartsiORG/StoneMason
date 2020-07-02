//
// Created by grant on 5/3/20.
//

#include "stms/timers.hpp"

#include <thread>
#include "stms/logging.hpp"

namespace stms {

    void Stopwatch::start() {
        startTime = std::chrono::steady_clock::now();
        state = setBit(state, 0) | setBit(state, 1);
    }

    void Stopwatch::stop() {
        stopTime = std::chrono::steady_clock::now();
        state = setBit(state, 2);
        state = resetBit8(state, 0);
    }

    float Stopwatch::getTime() {
        if (getBit(state, 0) && getBit(state, 1)) {
            return std::chrono::duration_cast<std::chrono::nanoseconds>(
                    std::chrono::steady_clock::now() - startTime).count() / 1000000.0f;
        } else if ((!getBit(state, 0)) && getBit(state, 1) && getBit(state, 2)) {
            return std::chrono::duration_cast<std::chrono::milliseconds>(stopTime - startTime).count() / 1000000.0f;
        }
        STMS_ERROR("Stopwatch::getTime() called when there is no time to get!");
        return 0;
    }

    void Stopwatch::reset() {
        startTime = std::chrono::steady_clock::now();
        setBit(state, 1);
        resetBit8(state, 2);
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
            return 99999.0f; // infinite tps
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
