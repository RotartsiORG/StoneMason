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

#ifdef STMS_ENABLE_LOGGING
#   define STMS_TRACE(...)  ::stms::insertLog(::stms::eTrace, __LINE__, __FILE__, __VA_ARGS__)
#   define STMS_DEBUG(...)  ::stms::insertLog(::stms::eDebug, __LINE__, __FILE__, __VA_ARGS__)
#   define STMS_INFO(...)   ::stms::insertLog(::stms::eInfo, __LINE__, __FILE__, __VA_ARGS__)
#   define STMS_WARN(...)   ::stms::insertLog(::stms::eWarn, __LINE__, __FILE__, __VA_ARGS__)
#   define STMS_ERROR(...)  ::stms::insertLog(::stms::eError, __LINE__, __FILE__, __VA_ARGS__)
#   define STMS_FATAL(...)  ::stms::insertLog(::stms::eFatal, __LINE__, __FILE__, __VA_ARGS__)
#else
#   define STMS_TRACE(...)
#   define STMS_DEBUG(...)
#   define STMS_INFO(...)
#   define STMS_WARN(...)
#   define STMS_ERROR(...)
#   define STMS_FATAL(...)
#endif

namespace stms {

    /**
     * Sets the thread pool to submit asynchronous logging tasks to. By calling this with a non-nullptr value,
     * asynchronous logging is enabled. If this is called with `nullptr`, async logging is disabled.
     * Async logging is disabled by default (since we don't know where to submit our tasks).
     * @param pool Pool to submit tasks to, or `nullptr`.
     */
    void setLogPool(ThreadPool *pool);

    void initLogging();

    enum LogLevel {
        eInvalid = 0,
        eTrace = 0b1,
        eDebug = 0b10,
        eInfo  = 0b100,
        eWarn  = 0b1000,
        eError = 0b10000,
        eFatal = 0b100000,
    };

    struct LogRecord {
        LogRecord(LogLevel lvl, std::time_t time, const char *file, unsigned line);

        LogLevel level = eInvalid;
        std::time_t time;

        const char *file = "";
        unsigned line{};

        fmt::memory_buffer msg;
    };

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

    static void consumeLogs() {
        getConsumingFlag() = true;

        bool empty = true;

        consumeAgain:

        getLogQueueMtx().lock();
        empty = getLogQueue().empty();
        if (!empty) {
            LogRecord top = std::move(getLogQueue().front());
            getLogQueue().pop();
            getLogQueueMtx().unlock();

            // formatting error: is `top.msg.data()` properly null-terminated?
            fmt::print("[{:%T %p %Z}] [{}:{}] [{}]: {}\n", *std::localtime(&top.time), top.file, top.line, top.level, top.msg.data());

            goto consumeAgain; // hopefully this compiles to better code than a while loop.
        } else {
            getLogQueueMtx().unlock();
        }

        getConsumingFlag() = false;
    }


    template <typename... Args>
    void insertLog(LogLevel lvl, unsigned line, const char *file, const char *fmtStr, const Args & ... args) {
        {
            std::lock_guard<std::mutex> lg(getLogQueueMtx());

            getLogQueue().emplace(
                    LogRecord{lvl, std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()), file, line});

            format_to(getLogQueue().back().msg, fmtStr, args...);
        }

        if (getLogThreadPool() != nullptr && !getConsumingFlag()) {
            getLogThreadPool()->submitTask(consumeLogs);
        } else if (!getConsumingFlag()) {
            consumeLogs(); // no async? just do it here.
        }
    }
}

//template <>
//struct fmt::formatter<stms::LogLevel> {
//    constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }
//
//    template <typename FormatContext>
//    auto format(const stms::LogLevel& d, FormatContext& ctx) {
//        switch (d) {
//            case (stms::eTrace): {
//                return format_to(ctx.out(), "trace");
//            } case (stms::eDebug): {
//                return format_to(ctx.out(), "debug");
//            } case (stms::eInfo): {
//                return format_to(ctx.out(), "info");
//            } case (stms::eWarn): {
//                return format_to(ctx.out(), "WARNING");
//            } case (stms::eError): {
//                return format_to(ctx.out(), "!ERROR!");
//            } case (stms::eFatal): {
//                return format_to(ctx.out(), "*FATAL*");
//            }
//            default: {
//                return format_to(ctx.out(), "!!INVALID LEVEL!!");
//            }
//        }
//    }
//};


#endif //__STONEMASON_LOGGING_HPP
