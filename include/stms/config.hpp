//
// Created by grant on 1/26/20.
//
#pragma once

#ifndef __STONEMASON_CONFIG_HPP
#define __STONEMASON_CONFIG_HPP

#define STMS_VERSION "0.0.1"

#define STMS_DEFAULT_WORKER_DELAY 125

#define STMS_ENABLE_LOGGING

#undef STMS_ENABLE_ASYNC_LOGGING

#define STMS_LOG_TO_LATEST

#define STMS_LOG_DIR "stms_logs"

#define STMS_LOG_FORMAT "%^[%H:%M:%S.%e] [%l] [%P|%t] [%!|%s:%#]: %v%$"

#define STMS_TIME_FORMAT "%Y.%m.%d %H:%M:%S"

#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE

#define STMS_LOG_TO_UNIQUE

namespace stms {

}

#endif //__STONEMASON_CONFIG_HPP
