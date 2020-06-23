//
// Created by grant on 1/25/20.
//

#include <thread>
#include <bitset>
#include <unordered_set>
#include <stms/curl.hpp>
#include "gtest/gtest.h"

#include "stms/audio.hpp"
#include "stms/config.hpp"
#include "stms/logging.hpp"
#include "stms/stms.hpp"

#include "stms/camera.hpp"
#include "glm/glm.hpp"

// Timeout after 10 seconds. The actual audio that we're playing is only 5 sec long.
constexpr unsigned alPlayBlockTimeout = 10;

namespace {
    TEST(AL, Play) {
        stms::initAll();
        stms::defaultAlContext().bind();

        stms::ALBuffer testBuf;
        stms::ALSource testSrc;
        testBuf.loadFromFile("./res/test.ogg");
        testSrc.enqueueBuf(&testBuf);
        testSrc.play();

        unsigned numSecs = 0;
        while (testSrc.isPlaying()) {
            std::this_thread::yield();
            std::this_thread::sleep_for(std::chrono::seconds(1));
            if (numSecs++ > alPlayBlockTimeout) {
                STMS_CRITICAL("AL Play timed out! This should only happen in Travis CI mac builds.");
                break;
            }
        }

        stms::handleAlError();
        stms::defaultAlDevice().handleError();
    }

    TEST(Log, Log) {
        stms::initAll();
        STMS_TRACE("STMS_TRACE: Pi is {1}, and the answer to life is {0}, I'm running STMS {2}", 42, 3.14,
                   stms::versionString);
        STMS_DEBUG("STMS_DEBUG");
        STMS_INFO("STMS_INFO");
        STMS_WARN("STMS_WARN");
        STMS_FATAL("STMS_FATAL, same as STMS_CRITICAL");
        STMS_CRITICAL("STMS_CRITICAL, same as STMS_FATAL");
    }

    TEST(UUID, Collisions) {
        for (int i = 0; i < 16; i++) {
            STMS_INFO("Sample UUID: {}", stms::genUUID4().buildStr());
        }

        const int numIter = 16384;
        std::unordered_set<std::string> seen;

        for (int i = 0; i < numIter; i++) {
            stms::UUID uuid = stms::genUUID4();
            std::string str = uuid.buildStr();

            std::bitset<8> uuidClockSeqHiAndReserved(uuid.clockSeqHiAndReserved);
            EXPECT_EQ(uuidClockSeqHiAndReserved.test(6), 0);
            EXPECT_EQ(uuidClockSeqHiAndReserved.test(7), 1);

            std::bitset<16> uuidTimeHiAndVersion(uuid.timeHiAndVersion);
            EXPECT_EQ(uuidTimeHiAndVersion.test(15), 0);
            EXPECT_EQ(uuidTimeHiAndVersion.test(14), 1);
            EXPECT_EQ(uuidTimeHiAndVersion.test(13), 0);
            EXPECT_EQ(uuidTimeHiAndVersion.test(12), 0);

            EXPECT_EQ(str.length(), 36);

            EXPECT_EQ(seen.find(str), seen.end()) << "UUID Collision for " << str;
            seen.emplace(str);
        }
    }

    TEST(CURL, ReadURL) {
        stms::ThreadPool pool;
        pool.start();

        auto future = stms::readURL("https://www.example.com", &pool);
        auto res = future.get();
        STMS_INFO("dat = '{}', size = {}", res.data, res.data.size());
        EXPECT_TRUE(res.success);
        EXPECT_GT(res.data.size(), 0);
        pool.stop();
    }

    TEST(Util, Hex) {
        EXPECT_EQ(stms::toHex(43981), "abcd");
        EXPECT_EQ(stms::toHex(65244, 6), "00fedc");
    }
}
