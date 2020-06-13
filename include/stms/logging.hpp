//
// Created by grant on 1/26/20.
//

#pragma once

#ifndef __STONEMASON_LOGGING_HPP
#define __STONEMASON_LOGGING_HPP

#include <sstream>
#include <iomanip>
#include <iostream>

#include "stms/config.hpp"

#ifdef STMS_ENABLE_LOGGING

#   include "spdlog/spdlog.h"
#   include "spdlog/sinks/stdout_color_sinks.h"
#   include "spdlog/sinks/basic_file_sink.h"
#   include "spdlog/async.h"
#   include "spdlog/sinks/msvc_sink.h"

#   ifdef __APPLE__
#       include "spdlog/sinks/syslog_sink.h"
#   endif

#   define STMS_TRACE SPDLOG_TRACE
#   define STMS_DEBUG SPDLOG_DEBUG
#   define STMS_INFO SPDLOG_INFO
#   define STMS_WARN SPDLOG_WARN
#   define STMS_CRITICAL SPDLOG_CRITICAL
#   define STMS_FATAL SPDLOG_CRITICAL
#else
#   define STMS_TRACE(...)
#   define STMS_DEBUG(...)
#   define STMS_INFO(...)
#   define STMS_WARN(...)
#   define STMS_FATAL(...)
#   define STMS_CRITICAL(...)
#endif

#include "spdlog/fmt/fmt.h" // TODO: Find a way to get rid of this dependency. Use C++20's fmtlib?

#define STMS_PUSH_ERROR(...) STMS_FATAL(__VA_ARGS__); ::stms::errorQueue.emplace_back(::stms::Error{__FILE__, __LINE__, ::stms::eError, fmt::format(__VA_ARGS__)});
#define STMS_PUSH_WARNING(...) STMS_WARN(__VA_ARGS__); ::stms::errorQueue.emplace_back(::stms::Error{__FILE__, __LINE__, ::stms::eWarning, fmt::format(__VA_ARGS__)});

namespace stms {
    enum ErrorType {
        eWarning, eError
    };


    struct Error {
        std::string file;
        unsigned line;
        ErrorType type;
        std::string msg;
    };

    extern std::vector<Error> errorQueue; // TODO: Use vector or actual std::queue? dequeue?

    std::string getCurrentDatetime();

    void initLogging();

}

#endif //__STONEMASON_LOGGING_HPP
