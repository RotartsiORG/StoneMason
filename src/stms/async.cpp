//
// Created by grant on 12/30/19.
//

#include <iostream>
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
            std::cerr << "`ThreadPool::start()` was called when thread pool is already running! Ignoring invocation!"
                      << std::endl;
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
            std::cerr << "`ThreadPool::stop()` was called when thread pool is already stopped! Ignoring invocation!"
                      << std::endl;
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

    void ThreadPool::pushWorker() {
        if (!this->running) {
            std::cerr << "`ThreadPool::pushWorker()` called while the thread pool was stopped! Ignoring invocation!"
                      << std::endl;
            return;
        }

        {
            std::lock_guard<std::recursive_mutex> lg(this->workerMtx);
            this->workers.emplace_back(workerFunc, this, this->workers.size() + 1);
        }
    }

    void ThreadPool::popWorker() {
        if (!this->running) {
            throw std::runtime_error("A worker was popped while the thread pool was stopped!");
        }

        std::thread back;
        {
            std::lock_guard<std::recursive_mutex> lg(this->workerMtx);
            back = std::move(this->workers.back());
            this->stopRequest = this->workers.size(); // Request the last worker to stop.
            this->workers.pop_back();
            if (this->workers.empty()) {
                this->running = false;
                std::cerr << "The last worker was popped from ThreadPool! Stopping the pool!" << std::endl;
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

        if (rhs.running) {
            std::cerr
                    << "A thread pool was moved (using `std::move`) whilst it was still running! This MAY cause issues!";
            std::cerr
                    << " The mutexes cannot be moved with the thread pool, therefore the mutex members are ignored!";
            std::cerr << " This MAY result in a race condition, and YOU will have to debug it!" << std::endl;
        }

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
            std::cerr << "Warning! Thread pool destroyed with " << tasks.size()
                      << " tasks still incomplete! They will NEVER get executed!"
                      << std::endl;
        }
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
}
