//
// Created by grant on 1/26/20.
//

#pragma once

#ifndef __STONEMASON_LOGGING_HPP
#define __STONEMASON_LOGGING_HPP

#include <sstream>
#include <iomanip>
#include <iostream>
#include <string>

#include "stms/config.hpp"
#include "stms/async.hpp"

#include <fmt/format.h>
#include <fmt/chrono.h>

namespace stms {

    enum LogLevel {
        eInvalid = 0,
        eTrace = 0b1,
        eDebug = 0b10,
        eInfo = 0b100,
        eWarn = 0b1000,
        eError = 0b10000,
        eFatal = 0b100000,
    };

    struct LogRecord {
        LogRecord(LogLevel lvl, std::chrono::system_clock::time_point time, const char *file, unsigned line);

        LogLevel level = eInvalid;
        std::chrono::system_clock::time_point time;

        const char *file = "";
        unsigned line{};

        fmt::memory_buffer msg;
    };

    void initLogging();

    void quitLogging();

#ifdef STMS_ENABLE_LOGGING
#   define STMS_TRACE(...)  ::stms::insertLog(::stms::eTrace, __LINE__, __FILE__, __VA_ARGS__)
#   define STMS_DEBUG(...)  ::stms::insertLog(::stms::eDebug, __LINE__, __FILE__, __VA_ARGS__)
#   define STMS_INFO(...)   ::stms::insertLog(::stms::eInfo, __LINE__, __FILE__, __VA_ARGS__)
#   define STMS_WARN(...)   ::stms::insertLog(::stms::eWarn, __LINE__, __FILE__, __VA_ARGS__)
#   define STMS_ERROR(...)  ::stms::insertLog(::stms::eError, __LINE__, __FILE__, __VA_ARGS__)
#   define STMS_FATAL(...)  ::stms::insertLog(::stms::eFatal, __LINE__, __FILE__, __VA_ARGS__)

//    static inline std::queue<LogRecord> &getLogQueue() {
//        static auto toProcess = std::queue<LogRecord>();
//        return toProcess;
//    }
//
//    static inline std::mutex &getLogQueueMtx() {
//        static auto mtx = std::mutex();
//        return mtx;
//    }
//
//    static inline ThreadPool *&getLogThreadPool() {
//        static ThreadPool *pool = nullptr;
//        return pool;
//    }
//
//    static inline volatile bool &getConsumingFlag() {
//        static volatile bool consuming = false;
//        return consuming;
//    }

    extern volatile bool logConsuming;
    extern std::mutex logQMtx;
    extern std::queue<LogRecord> logQueue;
    extern ThreadPool *logPool;
    extern std::vector<std::function<void(LogRecord *, std::string *)>> logHooks;

    /**
     * Sets the thread pool to submit asynchronous logging tasks to. By calling this with a non-nullptr value,
     * asynchronous logging is enabled. If this is called with `nullptr`, async logging is disabled.
     * Async logging is disabled by default (since we don't know where to submit our tasks).
     * @param pool Pool to submit tasks to, or `nullptr`.
     */
    inline void setLogPool(ThreadPool *pool) {
        logPool = pool;
    }

    static const char *logLevelToString(const LogLevel &lvl);

    void consumeLogs();

    template<typename... Args>
    void insertLog(LogLevel lvl, unsigned line, const char *file, const char *fmtStr, const Args &... args) {

        std::unique_lock<std::mutex> lg(logQMtx);

        logQueue.emplace(LogRecord{lvl, std::chrono::system_clock::now(), file, line});

        try {
            fmt::format_to(logQueue.back().msg, fmtStr, args...);
        } catch (fmt::format_error &e) {
            fmt::format_to(logQueue.back().msg,
                    "{}   \u001b[1m\u001b[31m!<<< FORMAT ERROR: {}\u001b[0m", fmtStr, e.what());  // screw it
        }

        if (logPool != nullptr && !logConsuming) {
            logConsuming = true;

            lg.unlock();

            logPool->submitTask(consumeLogs);

            if (!logPool->isRunning()) {
                logPool->start();
                std::cerr << "Logging thread pool was stopped! Starting it now!" << std::endl;
            }
        } else if (!logConsuming) {
            logConsuming = true;

            lg.unlock();

            consumeLogs(); // no async? just do it here.
        } else {
            lg.unlock();
        }
    }

#else
#   define STMS_TRACE(...)
#   define STMS_DEBUG(...)
#   define STMS_INFO(...)
#   define STMS_WARN(...)
#   define STMS_ERROR(...)
#   define STMS_FATAL(...)

    inline void setLogPool(ThreadPool *pool) {
        // no-op
    }

    inline void registerLogHook(const std::function<void(LogRecord *, std::string *)> &hook) {
        // no-op
    }
#endif
}

#endif //__STONEMASON_LOGGING_HPP
