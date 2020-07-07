//
// Created by grant on 1/26/20.
//
#pragma once

#ifndef __STONEMASON_CONFIG_HPP
#define __STONEMASON_CONFIG_HPP

#define STMS_ENABLE_LOGGING

/**
 * Builds StoneMason with OpenGL support. The corresponding option in CMakeLists.txt must also be set.
 */
#undef STMS_ENABLE_OPENGL
/**
 * Builds StoneMason with Vulkan support. The corresponding option in CMakeLists.txt must also be set.
 */
#define STMS_ENABLE_VULKAN

#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE

#undef STMS_NO_OPENSSL // TODO: Implement

namespace stms {
    constexpr char versionString[] = "0.0.0-dev2020.07.06";

    constexpr unsigned fullVersionNum = 1; // increment this every commit

    constexpr short versionMajor = 0;
    constexpr short versionMinor = 0;
    constexpr short versionMicro = 0;

    constexpr unsigned threadPoolWorkerDelayMs = 16; // 62 Hz

    constexpr bool logToLatestLog = true;
    constexpr bool logToUniqueFile = false;
    constexpr bool logToStdout = true;
    constexpr char logsDir[] = "./stms_logs"; // there must NOT be a trailing slash

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
