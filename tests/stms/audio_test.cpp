//
// Created by grant on 1/25/20.
//

#include <thread>
#include "gtest/gtest.h"
#include "stms/audio.hpp"

namespace {
    TEST(AL, Play) {
        stms::ALBuffer testBuf("/home/grant/Desktop/StoneMason/build/tests/test.ogg");
        stms::ALSource testSrc;
        testSrc.enqueueBuf(&testBuf);
        testSrc.play();

        while (testSrc.isPlaying()) {
            std::this_thread::yield();
            std::this_thread::sleep_for(std::chrono::milliseconds(125));
        }

        while (stms::handleAlError()) {
            FAIL() << "stms::handleAlError() failed!";
        }
        while (stms::defaultAlDevice.handleError()) {
            FAIL() << "stms::defaultAlDevice.handleError() failed!";
        }
    }
}