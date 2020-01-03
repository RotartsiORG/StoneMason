//
// Created by grant on 1/2/20.
//

#include "gtest/gtest.h"
#include "stms/async.hpp"

namespace {
    class ThreadPoolTests : public ::testing::Test {
    protected:
        stms::ThreadPool *pool = nullptr;

        void SetUp() override {
            this->pool = new stms::ThreadPool();
        }

        void TearDown() override {
            delete this->pool;
        }
    };

    void *factorialInPlace(void *dat) {
        int *x = reinterpret_cast<int *>(dat);
        int result = 1;
        for (int i = *x; i > 1; i--) {
            result *= i;
        }
        *x = result;
        return nullptr;
    }

    // This function was straight up copy-pasta'ed from GeeksForGeeks: https://www.geeksforgeeks.org/sieve-of-eratosthenes/
    // Just needed a quick algorithm to run.
    void *sieveOfEratosthenes(void *pInt) {
        int n = *reinterpret_cast<int *>(pInt);

        bool prime[n + 1];
        memset(prime, true, sizeof(prime));

        for (int p = 2; p * p <= n; p++) {
            if (prime[p]) {
                for (int i = p * p; i <= n; i += p) {
                    prime[i] = false;
                }
            }
        }

        auto *ret = new std::vector<int>();

        for (int p = 2; p <= n; p++) {
            if (prime[p]) {
                ret->emplace_back(p);
            }
        }

        return ret;
    }

    TEST_F(ThreadPoolTests, MoveTest) {
        stms::ThreadPool dis = std::move(*pool);
        *pool = std::move(dis);
    }

    TEST_F(ThreadPoolTests, TaskTest) {
        int a = 8;
        int b = 30;
        std::vector<int> expectedPrimes({2, 3, 5, 7, 11, 13, 17, 19, 23, 29});

        auto future0 = pool->submitTask(factorialInPlace, &a, 0);
        auto future1 = pool->submitTask(sieveOfEratosthenes, &b, 1);

        ASSERT_FALSE(pool->isRunning());
        pool->start();
        ASSERT_TRUE(pool->isRunning());

        std::vector<int> primes = *reinterpret_cast<std::vector<int> *>(future1.get());
        ASSERT_EQ(primes, expectedPrimes);

        ASSERT_EQ(nullptr, future0.get());
        ASSERT_EQ(40320, a);

        pool->stop();
        ASSERT_FALSE(pool->isRunning());
    }

    TEST_F(ThreadPoolTests, PopPushWorkerTest) {
        // TODO: Write this test
    }
}

