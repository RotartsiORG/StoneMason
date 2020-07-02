//
// Created by grant on 1/26/20.
//
#pragma once

#ifndef __STONEMASON_CONFIG_HPP
#define __STONEMASON_CONFIG_HPP

#define STMS_ENABLE_LOGGING

#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE

#undef STMS_NO_OPENSSL // TODO: Implement

namespace stms {
    constexpr char versionString[] = "0.0.1";

    constexpr int versionMajor = 0;
    constexpr int versionMinor = 0;
    constexpr int versionMicro = 1;

    constexpr unsigned threadPoolWorkerDelayMs = 16; // 62 Hz

    constexpr bool asyncLogging = false;
    constexpr bool logToLatest = true;
    constexpr bool logToUniqueFile = false;
    constexpr char logFormat[] = "%^[%H:%M:%S.%e] [%=8l] [%P|%t] [%!|%s:%#]: %v%$";
    constexpr char dateFormat[] = "%Y.%m.%d";
    constexpr char timeFormat[] = "%H:%M:%S %Z"; // 24 hour clock? :|
    constexpr char logDir[] = "./stms_logs/"; // must have trailing slash

    constexpr unsigned certAndCipherLen = 256;
    constexpr int waitEventsSleepAmount = 4;
    constexpr int sslShutdownMaxRetries = 10;
    constexpr int minIoTimeout = 1000;
    constexpr int tcpListenBacklog = 8;
    constexpr int maxRecvLen = 16384; // RFC 8449
    constexpr int secretCookieLen = 16;

    // Allow experimental driver features, useful for backwards compatibility.
    constexpr bool enableExperimentalGlew = true;
}


#endif //__STONEMASON_CONFIG_HPP
