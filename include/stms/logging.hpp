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

#include "spdlog/spdlog.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/async.h"
#include "spdlog/fmt/fmt.h"

#include "spdlog/sinks/msvc_sink.h"

#ifdef __APPLE__
#include "spdlog/sinks/syslog_sink.h"
#endif

#ifdef STMS_ENABLE_LOGGING

#define STMS_TRACE SPDLOG_TRACE

#define STMS_DEBUG SPDLOG_DEBUG

#define STMS_INFO SPDLOG_INFO

#define STMS_WARN SPDLOG_WARN

#define STMS_CRITICAL SPDLOG_CRITICAL

#define STMS_FATAL SPDLOG_CRITICAL

#else

#define STMS_TRACE(...)

#define STMS_DEBUG(...)

#define STMS_INFO(...)

#define STMS_WARN(...)

#define STMS_FATAL(...)

#define STMS_CRITICAL(...)

#endif


namespace stms {
    std::string getCurrentDatetime();

    static void onSPDLogError(const std::string &msg);

    class LogInitializer {
    public:
        uint8_t specialValue = 0;

        LogInitializer() noexcept;

        // Dummy function to make sure the constructor is called
        inline uint8_t init() volatile {
            std::cout << specialValue;
            return specialValue;
        }
    };

    extern volatile LogInitializer logging;

}

#endif //__STONEMASON_LOGGING_HPP
