//
// Created by grant on 7/25/20.
//

#include "gtest/gtest.h"
#include "stms/async.hpp"
#include "stms/logging.hpp"
#include "stms/stms.hpp"
#include "stms/config.hpp"

namespace {
    class LoggingTests : public ::testing::Test {
    protected:
        stms::ThreadPool *pool = nullptr;

        void SetUp() override {
            stms::initAll();
            this->pool = new stms::ThreadPool();
            this->pool->start(8);
            stms::getLogPool() = this->pool;
        }

        void TearDown() override {
            stms::getLogPool() = stms::getDefaultInstaPool();
            this->pool->waitIdle(1000);
            this->pool->stop();
            delete this->pool;
        }
    };

    TEST_F(LoggingTests, Log) {
        STMS_TRACE("STMS_TRACE: Pi is {1}, and the answer to life is {0}, I'm running STMS {2}", 42, 3.14,
                   stms::versionString);
        STMS_DEBUG("STMS_DEBUG");
        STMS_INFO("STMS_INFO");
        STMS_WARN("STMS_WARN");
        STMS_ERROR("STMS_ERROR");
        STMS_FATAL("STMS_FATAL");
    }

    TEST_F(LoggingTests, FlushOrder) {
        for (int i = 0; i < 10; i++) {
            STMS_TRACE("i = {}", i);
        }
    }

    TEST_F(LoggingTests, BadFmt) {
        const char *str = nullptr;
        STMS_INFO("This should fail {} {} {}", "not enough args");
        STMS_INFO("Oh no nullptr: {} {}", str, 0);
        STMS_INFO("Too many args: {}", 1, 2, 3);
        STMS_INFO("Mixing indexed: {1}, {0}, {}", 0, 1, 2);
    }
}
