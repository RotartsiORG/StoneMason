//
// Created by grant on 5/3/20.
//

#include "stms/timers.hpp"

#include <thread>

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

    unsigned Stopwatch::getTime() {
        if (getBit(state, 0) && getBit(state, 1)) {
            return std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - startTime).count();
        } else if ((!getBit(state, 0)) && getBit(state, 1) && getBit(state, 2)) {
            return std::chrono::duration_cast<std::chrono::milliseconds>(stopTime - startTime).count();
        } else {
            return 0;
        }
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

    unsigned TPSTimer::getLatestMspt() {
        return std::chrono::duration_cast<std::chrono::milliseconds>(latestTick - tickBefore).count();
    }

    unsigned TPSTimer::getLatestTps() {
        return 1000 / getLatestMspt();
    }

    void TPSTimer::wait(int fps) {
        auto now = std::chrono::steady_clock::now();
        auto dur = now - latestTick;
        auto targetDur = std::chrono::milliseconds(1000 / fps);
        if (targetDur > dur) {
            std::this_thread::sleep_for(targetDur - dur);
        }
    }
}
