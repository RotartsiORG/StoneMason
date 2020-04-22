//
// Created by grant on 1/25/20.
//

#include <thread>
#include <bitset>
#include "gtest/gtest.h"

#include "stms/audio.hpp"
#include "stms/config.hpp"
#include "stms/logging.hpp"
#include "stms/util.hpp"

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
        stms::initAll();
        STMS_TRACE("STMS_TRACE: Pi is {1}, and the answer to life is {0}, I'm running STMS {2}", 42, 3.14,
                   STMS_VERSION);
        STMS_DEBUG("STMS_DEBUG");
        STMS_INFO("STMS_INFO");
        STMS_WARN("STMS_WARN");
        STMS_FATAL("STMS_FATAL, same as STMS_CRITICAL");
        STMS_CRITICAL("STMS_CRITICAL, same as STMS_FATAL");
    }

    TEST(UUID, Collisions) {
        const int numIter = 16384;
        std::vector<std::string> seen;
        seen.resize(numIter);

        for (int i = 0; i < numIter; i++) {
            stms::UUID uuid = stms::genUUID4();
            uuid.getStr();

            std::bitset<8> uuidClockSeqHiAndReserved(uuid.clockSeqHiAndReserved);
            ASSERT_EQ(uuidClockSeqHiAndReserved.test(6), 0);
            ASSERT_EQ(uuidClockSeqHiAndReserved.test(7), 1);

            std::bitset<16> uuidTimeHiAndVersion(uuid.timeHiAndVersion);
            ASSERT_EQ(uuidTimeHiAndVersion.test(15), 0);
            ASSERT_EQ(uuidTimeHiAndVersion.test(14), 1);
            ASSERT_EQ(uuidTimeHiAndVersion.test(13), 0);
            ASSERT_EQ(uuidTimeHiAndVersion.test(12), 0);

            ASSERT_EQ(uuid.strCache.length(), 36);

            ASSERT_EQ(std::find(seen.begin(), seen.end(), uuid.strCache), seen.end())
                                        << "UUID Collision for " << uuid.strCache;
            seen.emplace_back(uuid.strCache);
        }
    }
}