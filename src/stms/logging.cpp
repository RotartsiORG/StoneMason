//
// Created by grant on 1/26/20.
//

#include "stms/logging.hpp"

#include <chrono>
#include <sys/stat.h>
#include <queue>
#include <mutex>

namespace stms {
#ifdef STMS_ENABLE_LOGGING

    void initLogging() {}

    void setLogPool(ThreadPool *pool) {
        getLogThreadPool() = pool;
    }

#else

    void initLogging() {};

    void setLogPool(ThreadPool *pool) {};
#endif

    LogRecord::LogRecord(LogLevel lvl, std::chrono::system_clock::time_point time, const char *file, unsigned int line)
                            : level(lvl), time(time), file(file), line(line) {}
}

