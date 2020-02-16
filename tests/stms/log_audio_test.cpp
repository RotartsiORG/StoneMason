//
// Created by grant on 1/25/20.
//

#include <thread>
#include "gtest/gtest.h"

#include "stms/audio.hpp"
#include "stms/config.hpp"
#include "stms/logging.hpp"

namespace {
    TEST(AL, Play) {
        stms::al::ALBuffer testBuf("./res/test.ogg");
        stms::al::ALSource testSrc;
        testSrc.enqueueBuf(&testBuf);
        testSrc.play();

        while (testSrc.isPlaying()) {
            std::this_thread::yield();
            std::this_thread::sleep_for(std::chrono::milliseconds(125));
        }

        while (stms::al::handleAlError()) {
            FAIL() << "stms::handleAlError() failed!";
        }
        while (stms::al::defaultAlDevice.handleError()) {
            FAIL() << "stms::defaultAlDevice.handleError() failed!";
        }
    }

    TEST(Log, Log) {
        stms::logging.init();
        STMS_TRACE("STMS_TRACE: Pi is {1}, and the answer to life is {0}, I'm running STMS {2}", 42, 3.14,
                   STMS_VERSION);
        STMS_DEBUG("STMS_DEBUG");
        STMS_INFO("STMS_INFO");
        STMS_WARN("STMS_WARN");
        STMS_FATAL("STMS_FATAL, same as STMS_CRITICAL");
        STMS_CRITICAL("STMS_CRITICAL, same as STMS_FATAL");
    }
}