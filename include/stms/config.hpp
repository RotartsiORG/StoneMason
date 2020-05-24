//
// Created by grant on 1/26/20.
//
#pragma once

#ifndef __STONEMASON_CONFIG_HPP
#define __STONEMASON_CONFIG_HPP

#define STMS_ENABLE_LOGGING

#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE


// Unless you plan on NEVER using cryptography or can stand UUID collisions, DO NOT set this to true.
#define STMS_IGNORE_BAD_RNG false

#undef STMS_NO_OPENSSL

namespace stms {
    constexpr char versionString[] = "0.0.1";

    constexpr int versionMajor = 0;
    constexpr int versionMinor = 0;
    constexpr int versionMicro = 1;

    constexpr unsigned threadPoolWorkerDelayMs = 125;

    constexpr bool asyncLogging = false;
    constexpr bool logToLatest = true;
    constexpr bool logToUniqueFile = false;
    constexpr char logFormat[] = "%^[%H:%M:%S.%e] [%=8l] [%P|%t] [%!|%s:%#]: %v%$";
    constexpr char timeFormat[] = "%Y.%m.%d %H:%M:%S";
    constexpr char logDir[] = "./stms_logs/"; // must have trailing slash

    namespace net {
        constexpr unsigned certAndCipherLen = 256;
        constexpr int sslShutdownMaxRetries = 10;
        constexpr int minIoTimeout = 1000;
        constexpr int maxConnectionTimeouts = 1;
        constexpr int threadPoolPriority = 8;
        constexpr int maxRecvLen = 16384; // RFC 8449
        constexpr int secretCookieLen = 16;
    }
}


#endif //__STONEMASON_CONFIG_HPP
