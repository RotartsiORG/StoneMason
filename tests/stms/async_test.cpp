//
// Created by grant on 1/2/20.
//

#include "gtest/gtest.h"
#include "stms/async.hpp"

namespace {
    class ThreadPoolTest : public ::testing::Test {
    protected:
        stms::ThreadPool *pool = nullptr;

        void SetUp() override {
            this->pool = new stms::ThreadPool();
        }

        virtual void TearDown() {
            delete this->pool;
        }
    };

    static void *factorialInPlace(void *dat) {
        int *x = reinterpret_cast<int *>(dat);
        int result = 1;
        for (int i = *x; i > 1; i--) {
            result *= i;
        }
        *x = result;
        return nullptr;
    }

    TEST_F(ThreadPoolTest, MoveTest
    ) {
//        stms::ThreadPool dis = std::move(*pool);
//        *pool = std::move(dis);
}

TEST_F(ThreadPoolTest, DoingStuffTest
) {
int a = 8;
std::packaged_task<void *(void *)> task(factorialInPlace);
std::future<void *> future = task.get_future();

pool->submitTask({
});
pool->submitTask({
8, &task, &a});
ASSERT_FALSE(pool
->

isRunning()

);
pool->

start();

ASSERT_TRUE(pool
->

isRunning()

);
ASSERT_EQ(nullptr, future.

get()

);
ASSERT_EQ(40320, a);
pool->

stop();

ASSERT_FALSE(pool
->

isRunning()

);
}
}

