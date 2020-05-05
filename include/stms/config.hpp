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

#define STMS_LOG_FORMAT "%^[%H:%M:%S.%e] [%=8l] [%P|%t] [%!|%s:%#]: %v%$"

#define STMS_TIME_FORMAT "%Y.%m.%d %H:%M:%S"

#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE

#undef STMS_LOG_TO_UNIQUE

// Unless you plan on NEVER using cryptography or can stand UUID collisions, DO NOT set this to true.
#define STMS_IGNORE_BAD_RNG false

#define STMS_DTLS_COOKIE_SECRET_LEN 16
#define STMS_DTLS_SEC_TIMEOUT 5
#define STMS_DTLS_USEC_TIMEOUT 0
#define STMS_DTLS_MAX_TIMEOUTS 1
// RFC 8449
#define STMS_DTLS_MAX_RECV_LEN 16384


namespace stms::net {
    static constexpr unsigned certAndCipherLen = 256;
}

#endif //__STONEMASON_CONFIG_HPP
