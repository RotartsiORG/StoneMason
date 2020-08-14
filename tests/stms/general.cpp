//
// Created by grant on 1/25/20.
//

#include <thread>
#include <bitset>
#include <unordered_set>
#include <stms/curl.hpp>
#include "gtest/gtest.h"

#include "stms/audio.hpp"
#include "stms/logging.hpp"
#include "stms/stms.hpp"
#include "stms/camera.hpp"
#include "stms/timers.hpp"

#include "stms/except.hpp"

// Timeout after 10 seconds. The actual audio that we're playing is only 5 sec long.
constexpr unsigned alPlayBlockTimeout = 10;

constexpr float floatingPointThreshold = 0.0625f; // Fucking floating point rounding errors

#define STMS_FLOAT_EQ(a, b, c) cmpFloatArrs(reinterpret_cast<const float *>(&a), reinterpret_cast<const float *>(&b), c)

namespace {
    void cmpFloatArrs(const float *a, const float *b, unsigned size) {
        for (unsigned i = 0; i < size; i++) {
            EXPECT_GT(*(a + i), (*(b + i)) - floatingPointThreshold);
            EXPECT_LT(*(a + i), (*(b + i)) + floatingPointThreshold);
        }
    }

    TEST(AL, Play) {
        stms::initAll();
        try {
            stms::defaultAlContext().bind();
        } catch (std::runtime_error &e) {
            STMS_INFO("Got runtime error {}", e.what());
        }

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
                STMS_ERROR("AL Play timed out! This should only happen in Travis CI mac builds.");
                break;
            }
        }

        // Use handle instead of flush bc al would bug out and it would get stuck in a loop
        stms::handleAlError();
        stms::defaultAlDevice().handleError();
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

        auto handle = stms::CURLHandle(&pool);
        handle.setUrl("https://www.example.com");
        auto future = handle.perform();

        auto res = future.get();
        STMS_INFO("dat = '{}', size = {}", handle.result, handle.result.size());
        EXPECT_FALSE(res);
        EXPECT_GT(handle.result.size(), 0);
        pool.stop();
    }

    TEST(Util, Hex) {
        EXPECT_EQ(stms::toHex(43981), "abcd");
        EXPECT_EQ(stms::toHex(65244, 6), "00fedc");
    }

    TEST(Camera, TransformInfo) {

        stms::TransformInfo ti{};
        ti.buildAll();

        // By default, an identity matrix should be built
        STMS_FLOAT_EQ(ti.mat4, stms::idMat, 16);

        ti.setEuler({0, 0, 3.1415});
        ti.pos = {0, 100, 0};
        ti.scale = {0.1f, 1, -1};
        ti.buildAll();

        stms::TransformInfo tic = ti;

        glm::vec4 orig = {100, 0, 5, 1}; // -10 100 -5 1
        orig = tic.mat4 * orig;

        auto target = glm::vec4(-10, 100, -5, 1);
        STMS_FLOAT_EQ(orig, target, 4);

        stms::Camera cam{};
        cam.trans = ti;
        cam.buildAll();

        // Roundabout way to test if they are the inverse of each other
        auto camMulTic = cam.matV * tic.mat4;
        auto ticMulCam = tic.mat4 * cam.matV;

        STMS_FLOAT_EQ(ti.mat4, tic.mat4, 16);
        STMS_FLOAT_EQ(camMulTic, ticMulCam, 16);
        STMS_FLOAT_EQ(camMulTic, stms::idMat, 16);
    }

    TEST(Timers, Stopwatch) {
        constexpr unsigned char msThreshold = 25; // REALLY lenient bc travis
        constexpr unsigned char waitAmount = 125;

        stms::Stopwatch sp;
        EXPECT_FALSE(sp.isRunning());
        EXPECT_EQ(sp.getTime(), 0);

        if (stms::exceptionLevel > 1) {
            EXPECT_THROW(sp.reset(), std::runtime_error);
        }

        sp.start();
        EXPECT_TRUE(sp.isRunning());
        std::this_thread::sleep_for(std::chrono::milliseconds(waitAmount));
        auto t = sp.getTime();
        EXPECT_GT(t, waitAmount - msThreshold);
        if (t >= waitAmount + msThreshold) {
            STMS_WARN("Stopwatch test was slower than expected! Expected {}ms but got {}ms", waitAmount, t);
        }

        sp.reset();
        std::this_thread::sleep_for(std::chrono::milliseconds(waitAmount));
        sp.stop();
        EXPECT_FALSE(sp.isRunning());
        std::this_thread::sleep_for(std::chrono::milliseconds(waitAmount));

        t = sp.getTime();
        EXPECT_GT(t, waitAmount - msThreshold);
        if (t >= waitAmount + msThreshold) {
            STMS_WARN("Stopwatch test was slower than expected! Expected {}ms but got {}ms", waitAmount, t);
        }

        auto cpy = sp;
        EXPECT_EQ(cpy.getTime(), sp.getTime());
        EXPECT_EQ(cpy.isRunning(), sp.isRunning());
    }

    TEST(Timers, TPSTimer) {
        // Let's be REALLY lenient because travis could lag badly
        constexpr float msptThreshold = 3.0f;
        constexpr float tpsThreshold = 5.0f;

        stms::TPSTimer tm;
        EXPECT_EQ(tm.getLatestMspt(), 0);
        EXPECT_EQ(tm.getLatestTps(), FLT_MAX);

        for (int targetFps = 30; targetFps <= 120; targetFps += 30) {
            auto floatTarget = static_cast<float>(targetFps);
            for (unsigned i = 0; i < 15; i++) {
                tm.tick();
                tm.wait(floatTarget);
            }

            auto ret = tm.getLatestTps();
            EXPECT_LT(ret, floatTarget + tpsThreshold);
            if (ret <= floatTarget - tpsThreshold) {
                STMS_WARN("TPSTimer test was slower than expected! Got {} TPS (expected {})", ret, floatTarget);
            }

            auto targetMspt = 1000.0f / floatTarget;
            ret = tm.getLatestMspt();
            EXPECT_GT(ret, targetMspt - msptThreshold);
            if (ret >= targetMspt + msptThreshold) {
                STMS_WARN("TPSTimer test was slower than expected! Got {} MSPT (expected {})", ret, targetMspt);
            }
        }

        auto cpy = tm;
        EXPECT_EQ(tm.getLatestMspt(), cpy.getLatestMspt());
        EXPECT_EQ(tm.getLatestTps(), cpy.getLatestTps());
    }

    TEST(Util, ReadFile) {
        auto contents = stms::readFile("./res/test.txt");
        STMS_INFO("Read '{}'", contents);
        EXPECT_EQ(contents, "begin-Lorem Ipsum-end");
    }
}
