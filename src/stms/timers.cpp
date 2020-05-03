//
// Created by grant on 5/3/20.
//

#include "stms/timers.hpp"

namespace stms {

    void Stopwatch::start() {
        startTime = std::chrono::steady_clock::now();
        state.setBit(1, true);
        state.setBit(0, true);
    }

    void Stopwatch::stop() {
        stopTime = std::chrono::steady_clock::now();
        state.setBit(2, true);
        state.setBit(0, false);
    }

    unsigned Stopwatch::getTime() {
        if (state.getBit(0) && state.getBit(1)) {
            return std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - startTime).count();
        } else if ((!state.getBit(0)) && state.getBit(1) && state.getBit(2)) {
            return std::chrono::duration_cast<std::chrono::milliseconds>(stopTime - startTime).count();
        } else {
            return 0;
        }
    }

    void Stopwatch::reset() {
        startTime = std::chrono::steady_clock::now();
        state.setBit(1, true);
        state.setBit(2, false);
    }

    void TPSTimer::tick() {
        tickBefore = latestTick;
        latestTick = std::chrono::steady_clock::now();
    }

    unsigned TPSTimer::getLatestMspt() {
        return std::chrono::duration_cast<std::chrono::milliseconds>(latestTick - tickBefore).count();
    }

    unsigned TPSTimer::getLatestTps() {
        return 1000 / getLatestMspt();
    }
}
