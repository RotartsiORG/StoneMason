//
// Created by grant on 12/30/19.
//

#include <iostream>
#include <stms/logging.hpp>
#include "stms/async.hpp"

namespace stms {
    static void workerFunc(ThreadPool *parent, size_t index) {
        while (parent->running) {
            if (index == parent->stopRequest) {
                parent->stopRequest = 0;
                return;
            }

            parent->taskQueueMtx.lock();
            if (parent->tasks.empty()) {
                parent->taskQueueMtx.unlock();
                std::this_thread::yield();
                std::this_thread::sleep_for(std::chrono::milliseconds(parent->workerDelay));
                continue;
            } else {
                ThreadPool::ThreadPoolTask front = std::move(
                        *const_cast<ThreadPool::ThreadPoolTask *>(&parent->tasks.top()));

                parent->tasks.pop();
                parent->taskQueueMtx.unlock();

                front.task(front.pData);
            }
        }
    }

    ThreadPool::TaskComparator::TaskComparator(const bool &rev) : reverse(rev) {}

    bool ThreadPool::TaskComparator::operator()(const ThreadPoolTask &lhs, const ThreadPoolTask &rhs) const {
        if (this->reverse) {
            return (lhs.priority > rhs.priority);
        } else {
            return (lhs.priority < rhs.priority);
        }
    }


    void ThreadPool::start(unsigned threads) {
        if (this->running) {
            STMS_INFO("`ThreadPool::start()` was called when thread pool is already running! Ignoring invocation!");
            return;
        }
        if (threads == 0) {
            threads = (unsigned) (std::thread::hardware_concurrency() * 1); // Choose a good multiplier
            if (threads == 0) { // If that's STILL 0, default to 8 threads.
                threads = 8;
            }
        }

        this->running = true;
        {
            std::lock_guard<std::recursive_mutex> lg(this->workerMtx);
            for (unsigned i = 0; i < threads; i++) {
                this->workers.emplace_back(std::thread(workerFunc, this, i + 1));
            }
        }
    }

    void ThreadPool::stop() {
        if (!this->running) {
            STMS_INFO("`ThreadPool::stop()` was called when thread pool is already stopped! Ignoring invocation!");
            return;
        }
        this->running = false;

        bool workersEmpty;

        {
            std::lock_guard<std::recursive_mutex> lg(this->workerMtx);
            workersEmpty = this->workers.empty();
        }

        while (!workersEmpty) {
            std::thread front;

            {
                std::lock_guard<std::recursive_mutex> lg(this->workerMtx);
                front = std::move(this->workers.front());
                this->workers.pop_front();
                workersEmpty = this->workers.empty();
            }

            if (front.joinable()) {
                front.join();
            } else {
                front.detach();  // Simply forget thread if not joinable.
            }
        }
    }

    std::future<void *> ThreadPool::submitTask(std::function<void *(void *)> func, void *dat, unsigned priority) {
        ThreadPoolTask task{};
        task.pData = dat;
        task.priority = priority;
        task.task = std::packaged_task<void *(void *)>(func);

        // Save future to variable since `task` is moved.
        auto future = task.task.get_future();

        std::lock_guard<std::recursive_mutex> lg(this->taskQueueMtx);
        this->tasks.push(std::move(task));

        return future;
    }

    void ThreadPool::pushThread() {
        if (!this->running) {
            STMS_DEBUG(
                    "`ThreadPool::pushThread()` called while the thread pool was stopped! Starting the thread pool!");
            this->running = true;
        }

        {
            std::lock_guard<std::recursive_mutex> lg(this->workerMtx);
            this->workers.emplace_back(workerFunc, this, this->workers.size() + 1);
        }
    }

    void ThreadPool::popThread() {
        if (!this->running) {
            STMS_INFO("`ThreadPool::popThread()` was called while the thread pool was stopped! Ignoring invocation");
            return;
        }

        std::thread back;
        {
            std::lock_guard<std::recursive_mutex> lg(this->workerMtx);
            back = std::move(this->workers.back());
            this->stopRequest = this->workers.size(); // Request the last worker to stop.
            this->workers.pop_back();
            if (this->workers.empty()) {
                STMS_DEBUG("The last thread was popped from ThreadPool! Stopping the pool!");
                this->running = false;
            }
        }

        if (back.joinable()) {
            back.join();
        } else {
            back.detach();
        }
    }

    ThreadPool &ThreadPool::operator=(ThreadPool &&rhs) noexcept {
        if (&rhs == this) {
            return *this;
        }

        // Hopefully locking ALL mutexes during the move is enough to prevent aforementioned race conditions.
        std::lock_guard<std::recursive_mutex> rhsWorkerLg(rhs.workerMtx);
        std::lock_guard<std::recursive_mutex> thisWorkerLg(this->workerMtx);
        std::lock_guard<std::recursive_mutex> rhsTaskLg(rhs.taskQueueMtx);
        std::lock_guard<std::recursive_mutex> thisTaskLg(this->taskQueueMtx);

        this->destroy();

        // We cannot move the mutex so we quietly skip it and hope nobody notices. (Watch it crash and burn later)
        this->stopRequest = rhs.stopRequest;
        this->running = rhs.running;
        this->workerDelay = rhs.workerDelay;
        this->tasks = std::move(rhs.tasks);
        this->workers = std::move(rhs.workers);

        return *this;
    }

    ThreadPool::ThreadPool(ThreadPool &&rhs) noexcept {
        *this = std::move(rhs);
    }

    ThreadPool::~ThreadPool() {
        this->destroy();
    }

    void ThreadPool::waitIdle() {
        while (getNumTasks() != 0) {
            std::this_thread::yield();
            std::this_thread::sleep_for(std::chrono::milliseconds(workerDelay));
        }
    }

    void ThreadPool::destroy() {
        if (this->running) {
            stop();
        }
        std::lock_guard<std::recursive_mutex> lg(this->taskQueueMtx);
        if (!tasks.empty()) {
            STMS_WARN(
                    "Warning! A thread pool was destroyed with {} tasks still incomplete! They will NEVER be executed!",
                    tasks.size());
        }
    }

    void ThreadPool::rawSubmit(std::packaged_task<void *(void *)> *task, void *data, unsigned priority) {
        ThreadPoolTask obj{};
        obj.pData = data;
        obj.priority = priority;
        obj.task = std::move(*task);

        std::lock_guard<std::recursive_mutex> lg(this->taskQueueMtx);
        this->tasks.push(std::move(obj));
    }

    ThreadPool::ThreadPoolTask::ThreadPoolTask(ThreadPool::ThreadPoolTask &&rhs) noexcept {
        *this = std::move(rhs);
    }

    ThreadPool::ThreadPoolTask &ThreadPool::ThreadPoolTask::operator=(ThreadPool::ThreadPoolTask &&rhs) noexcept {
        if (&rhs == this) {
            return *this;
        }

        this->pData = rhs.pData;
        this->priority = rhs.priority;
        this->task = std::move(rhs.task);

        return *this;
    }


    IOService::IOService() {

    }

    IOService::~IOService() {

    }

    unsigned IOService::addPool(ThreadPool *pool, unsigned int min, unsigned int max) {
        PoolAndRange obj{};
        obj.id = nextPoolID++;
        obj.minPriority = min,
                obj.maxPriority = max;
        obj.threadPool = pool;

        pools.emplace_back(obj);
        return obj.id;
    }

    void IOService::removePool(unsigned pool) {
        for (size_t i = 0; i < pools.size(); i++) {
            if (pools.at(i).id == pool) {
                pools.erase(pools.begin() + i);
                return;
            }
        }
        STMS_INFO("`IOService::removePool` called on non-existent pool {}! Ignoring invocation.", pool);
    }

    std::future<void *> IOService::submitTask(std::function<void *(void *)> func, void *dat, unsigned priority) {
        auto packagedTask = std::packaged_task<void *(void *)>(func);

        // Save future to variable since rawSubmit() moves the task.
        auto future = packagedTask.get_future();

        rawSubmit(&packagedTask, dat, priority);
        return future;
    }

    void IOService::rawSubmit(std::packaged_task<void *(void *)> *task, void *data, unsigned priority) {
        PoolAndRange targetPool{};
        for (auto pool : pools) {
            if (pool.minPriority <= priority && pool.maxPriority >= priority) {
                if (targetPool.threadPool == nullptr) {
                    targetPool = pool;
                } else if (pool.threadPool->getNumTasks() < targetPool.threadPool->getNumTasks()) {
                    targetPool = pool;
                }
            }
        }

        targetPool.threadPool->rawSubmit(task, data, priority);
    }

    unsigned IOService::setInterval(const std::function<void(void *)> &func, void *data, bool timeExecution) {
        Interval obj{};
        obj.id = nextIntervalID++;
        obj.data = data;
        obj.func = func;
        obj.timeExec = timeExecution;

        // TODO: Do the thread stuff here.

        intervals.emplace_back(obj);
        return obj.id;
    }

    void IOService::clearInterval(unsigned interval) {
        for (size_t i = 0; i < intervals.size(); i++) {
            if (intervals.at(i).id == interval) {
                // TODO: Properly handle threads here.
                intervals.erase(intervals.begin() + i);
                return;
            }
        }
        STMS_INFO("`IOService::clearInterval` called on non-existent interval {}! Ignoring invocation.", interval);
    }

    std::future<void *>
    IOService::setTimeout(std::function<void *(void *)> func, void *data, unsigned delay, unsigned priority,
                          unsigned int *timeoutID) {
        Timeout obj{};
        obj.id = nextTiemoutID++;
        obj.data = data;
        obj.task = std::packaged_task<void *(void *)>(func);
        obj.delay = delay;
        obj.start = std::chrono::steady_clock::now();
        obj.priority = priority;

        std::future<void *> ret = obj.task.get_future();
        *timeoutID = obj.id;

        timeouts.emplace_back(std::move(obj));
        return ret;
    }

    void IOService::clearTimeout(unsigned timeout) {
        for (size_t i = 0; i < timeouts.size(); i++) {
            if (timeouts.at(i).id == timeout) {
                timeouts.erase(timeouts.begin() + i);
                return;
            }
        }
        STMS_INFO("`IOService::clearTimeout` called on non-existent timeout {}! Ignoring invocation.", timeout);
    }

    IOService &IOService::operator=(IOService &&rhs) noexcept {
        if (this == &rhs) {
            return *this;
        }

        /// TODO: Write operator = stuff here

        return *this;
    }

    IOService::IOService(IOService &&rhs) noexcept {
        *this = std::move(rhs);
    }

    void IOService::update() {
        for (size_t i = 0; i < timeouts.size(); i++) {
            auto timeSoFar = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - timeouts.at(i).start);
            if (timeSoFar.count() >= timeouts.at(i).delay) {
                rawSubmit(&timeouts.at(i).task, timeouts.at(i).data, timeouts.at(i).priority);
                timeouts.erase(timeouts.begin() + i);
            }
        }

        // TODO: Updated intervals

    }
}
