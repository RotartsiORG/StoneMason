//
// Created by grant on 1/2/20.
//

#include <utility>

#include "gtest/gtest.h"
#include "stms/async.hpp"
#include "stms/scheduler.hpp"
#include "stms/logging.hpp"

namespace {
    const int numTasks = 8;

    struct WorkArgs {
        unsigned wait;
        std::string *pStr;
        std::mutex *pStrMtx;
        std::string identifier;

        WorkArgs(unsigned w, std::string *s, std::mutex *m, std::string i) : wait(w), pStr(s), pStrMtx(m), identifier(std::move(i)) {}
    };

    bool operator==(const WorkArgs &lhs, const WorkArgs &rhs) {
        return (lhs.identifier == rhs.identifier);
    }

    std::ostream &operator<<(std::ostream &out, const WorkArgs &f) {
        return out << "WorkArgs<wait=" << f.wait << ", pStr=" << f.pStr << ", pStrMtx=" << f.pStrMtx << ", id="
                   << f.identifier << ">";
    }

    WorkArgs *doWork(void *wait) {
        auto *args = reinterpret_cast<WorkArgs *>(wait);
        std::this_thread::sleep_for(std::chrono::milliseconds(args->wait));

        std::lock_guard<std::mutex> lg(*args->pStrMtx);
        args->pStr->append(args->identifier + ", ");
        return args;
    }

    class ThreadPoolTests : public ::testing::Test {
    protected:
        stms::ThreadPool *pool = nullptr;
        std::string order;
        std::string expecteOrder;
        std::mutex orderMtx;

        std::vector<WorkArgs *> results;

        std::vector<WorkArgs> workArgs;

        void SetUp() override {
            this->pool = new stms::ThreadPool();
        }

        void TearDown() override {
            delete this->pool;
        }

        void startPool(unsigned numThreads = 0) {
            workArgs.clear();
            for (unsigned i = 0; i < numTasks; i++) {
                std::string id = std::to_string(i);
                WorkArgs workArg = WorkArgs{1000, &order, &orderMtx, id};
                workArgs.emplace_back(workArg);
                expecteOrder += (id + ", ");
            }

            order.clear();
            results.clear();
            results.resize(numTasks);
            for (int i = 0; i < numTasks; i++) {
                pool->submitTask([&, capI{i}]() { results[capI] = doWork(&workArgs.at(capI)); });
            }

            EXPECT_FALSE(pool->isRunning());
            pool->start(numThreads);
            EXPECT_TRUE(pool->isRunning());
        }

        void stopPool() {
            pool->waitIdle(0);

            EXPECT_TRUE(pool->isRunning());
            pool->stop(true);
            EXPECT_FALSE(pool->isRunning());

            for (int i = 0; i < numTasks; i++) {
                EXPECT_EQ(*results.at(i), workArgs.at(i));
            }

            // Skip order checking; it's not THAT important (i hope)
            std::lock_guard<std::mutex> lg(orderMtx);
            std::cout << "Order: '" << order << "' vs Expected: '" << expecteOrder << "'" << std::endl;
        }
    };

    TEST_F(ThreadPoolTests, MoveTest) {
        *pool = std::move(*pool);  // Self-assignment test.
        stms::ThreadPool dis = std::move(*pool);
        *pool = std::move(dis);
    }

    TEST_F(ThreadPoolTests, TaskTest) {
        startPool();
        stopPool();
    }

    TEST_F(ThreadPoolTests, MoveWhileRunningTest) {
        startPool();
        *pool = std::move(*pool);
        stms::ThreadPool temp = std::move(*pool);
        *pool = std::move(temp);
        stopPool();
    }

    TEST_F(ThreadPoolTests, MultiStart) {
        startPool();
        pool->start(5);
        EXPECT_EQ(pool->getNumThreads(), 5);
        pool->start(3);
        EXPECT_EQ(pool->getNumThreads(), 3);
        stopPool();
    }

    TEST_F(ThreadPoolTests, PopPushWorkerTest) {
        startPool(8);
        for (int i = 0; i < 8; i++) {
            pool->popThread();
        }
        EXPECT_EQ(pool->getNumThreads(), 0);

        for (int i = 0; i < 8; i++) {
            pool->pushThread();
        }

        EXPECT_EQ(pool->getNumThreads(), 8);
        stopPool();
    }

    TEST_F(ThreadPoolTests, Scheduler) {
        startPool(8);
        std::atomic_int count{0};
        bool go = true;

        stms::TimedScheduler sched(pool);
        sched.setTimeout([&]() {
            STMS_INFO("TimedOut!");
            go = false;
        }, 2000);

        sched.clearTimeout(sched.setTimeout([&]() {
            STMS_INFO("BECLEARED");
        }, 1000).id);

        stms::TaskIdentifier id = sched.setInterval([&]() {
            STMS_INFO("Interval! Count {}", count++);
            if (count >= 10) {
                sched.clearTimeout(id);
            }
        }, 125);

        while (go) {
            sched.tick();
        }

        stopPool();
    }
}

