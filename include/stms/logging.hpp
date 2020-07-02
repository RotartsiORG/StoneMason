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

    static std::queue<LogRecord> &getLogQueue() {
        static auto toProcess = std::queue<LogRecord>();
        return toProcess;
    }

    static std::mutex &getLogQueueMtx() {
        static auto mtx = std::mutex();
        return mtx;
    }

    static ThreadPool *&getLogThreadPool() {
        static ThreadPool *pool = nullptr;
        return pool;
    }

    static bool &getConsumingFlag() {
        static bool consuming = false;
        return consuming;
    }

    /**
     * Gets the `std::vector` of log hooks for I/O. You may modify it to add your own hooks.
     *
     * @return Reference to the vector of log hooks.
     */
    inline std::vector<std::function<void(LogRecord *, std::string *)>> &getLogHooks() {
        static auto hooks = std::vector<std::function<void(LogRecord *, std::string *)>>();
        return hooks;
    }

    /**
     * Sets the thread pool to submit asynchronous logging tasks to. By calling this with a non-nullptr value,
     * asynchronous logging is enabled. If this is called with `nullptr`, async logging is disabled.
     * Async logging is disabled by default (since we don't know where to submit our tasks).
     * @param pool Pool to submit tasks to, or `nullptr`.
     */
    inline void setLogPool(ThreadPool *pool) {
        getLogThreadPool() = pool;
    }

    inline static const char *logLevelToString(const LogLevel &lvl) {
        switch (lvl) {
            case (eTrace):
                return "  \u001b[37mtrace\u001b[0m  "; // white
            case (eDebug):
                return "  \u001b[36mdebug\u001b[0m  "; // cyan
            case (eInfo):
                return "  \u001b[34minfo\u001b[0m   "; // blue
            case (eWarn):
                return " \u001b[1m\u001b[33mWARNING\u001b[0m "; // bold yellow
            case (eError):
                return "  \u001b[1m\u001b[31mERROR\u001b[0m  "; // bold red
            case (eFatal):
                return " \u001b[1m\u001b[4m\u001b[31m*FATAL*\u001b[0m "; // bold underlined red
            default:
                return "!!! INVALID LOG LEVEL !!!";
        }
    }

    static void consumeLogs() {

        getLogQueueMtx().lock();

        bool empty = getLogQueue().empty();
        if (!empty) {
            LogRecord top = std::move(getLogQueue().front());
            getLogQueue().pop();

            getLogQueueMtx().unlock();

            // formatting error: is `top.msg.data()` properly null-terminated?
            fmt::memory_buffer logMsg;

            fmt::memory_buffer fileUrl;
            format_to(fileUrl, "file://{}:{}", top.file, top.line);

            time_t localtimeReady = std::chrono::system_clock::to_time_t(top.time);
            auto seconds = std::chrono::time_point_cast<std::chrono::seconds>(top.time);
            auto ms = std::chrono::duration_cast<std::chrono::nanoseconds>(top.time - seconds);

            format_to(logMsg, "[{0:%T}.{1:<12}] [{2:^72}] [{3:<8}]: {4}", *std::localtime(&localtimeReady),
                      ms.count(),
                      to_string(fileUrl), logLevelToString(top.level), to_string(top.msg));

            std::string finalMsg = to_string(logMsg); // don't flush!

            for (const auto &func : getLogHooks()) {
                func(&top, &finalMsg);
            }

            // Recurse. No need to check/set the consume flag as they are only modified on exit/enter.
            // This is better than just looping bc it breaks the consume task up into multiple submits
            // to the thread pool!
            if (getLogThreadPool() != nullptr) {
                getLogThreadPool()->submitTask(consumeLogs);
            } else {
                consumeLogs();
            }
        } else {
            getConsumingFlag() = false;

            getLogQueueMtx().unlock();
        }
    }

    template<typename... Args>
    void insertLog(LogLevel lvl, unsigned line, const char *file, const char *fmtStr, const Args &... args) {

        getLogQueueMtx().lock();

        getLogQueue().emplace(LogRecord{lvl, std::chrono::system_clock::now(), file, line});

        format_to(getLogQueue().back().msg, fmtStr, args...);

        if (getLogThreadPool() != nullptr && !getConsumingFlag()) {
            getConsumingFlag() = true;

            getLogQueueMtx().unlock();

            getLogThreadPool()->submitTask(consumeLogs);
        } else if (!getConsumingFlag()) {
            getConsumingFlag() = true;

            getLogQueueMtx().unlock();

            consumeLogs(); // no async? just do it here.
        } else {
            getLogQueueMtx().unlock();
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
