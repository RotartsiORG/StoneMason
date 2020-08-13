/**
 * @file stms/config.hpp
 * @brief Configuration for StoneMason. Full of variables and macros you can tweak.
 * Created by grant on 1/26/20.
 */

#pragma once

#ifndef __STONEMASON_CONFIG_HPP
#define __STONEMASON_CONFIG_HPP
//!< Include guard

/// If not defined, all `STMS_XXX` log macros will resolve to nothing. Easily turn logging on/off
#define STMS_ENABLE_LOGGING

/// Namespace containing everything StoneMason has to offer! (https://github.com/RotartsiORG/StoneMason)
namespace stms {
    constexpr char versionString[] = "0.0.0-dev2020.08.12"; //!< Full version string for this version of StoneMason
    constexpr unsigned fullVersionNum = 5; //!< Raw version number, to be incremented every commit.

    /**
     * @var maturity
     * @brief This number represents the stability ("maturity") of this version. This will be subject to change.
     *          16: development version,
     *          32: alpha,
     *          48: beta,
     *          64: release candidate,
     *          80: production,
     *          96: LTS
     */
    constexpr unsigned maturity = 16;

    constexpr short versionMajor = 0; //!< Major number (X.x.x)
    constexpr short versionMinor = 0; //!< Minor number (x.X.x)
    constexpr short versionMicro = 0; //!< Micro/Patch number (x.x.X)

    constexpr unsigned threadPoolConvarTimeoutMs = 250; //!< Max number of milliseconds the condition var in worker threads would block

    constexpr bool logToLatestLog = true; //!< If true, write log output to `latest.log`
    constexpr bool logToUniqueFile = false; //!< If true, write log output to `<logsDir>/<datetime>.log`
    constexpr bool logToStdout = true; //!< If true, write log output to stdout
    constexpr char logsDir[] = "./stms_logs"; //!< Directory for log output. There must NOT be a trailing slash

    constexpr unsigned certAndCipherLen = 256; //!< Size of the string to allocate for logging OpenSSL certs and ciphers
    constexpr int waitEventsSleepAmount = 4; //!< Milliseconds to pause for on `waitEvents` so that newly connected clients are visible.
    constexpr int sslShutdownMaxRetries = 10; //!< Maximum amount of times to retry `SSL_shutdown` (see OpenSSL docs)
    constexpr int minIoTimeout = 1000; //!< Minimum amount of time to wait for pending TLS IO (in milliseconds).
    constexpr int tcpListenBacklog = 8; //!< The size of the TCP `accept` backlog. See man page for `listen`.
    constexpr int maxRecvLen = 16384; //!< The size (bytes) of the buffer to allocate for incoming TLS packets. RFC 8449
    constexpr int secretCookieLen = 16; //!< Length of DTLS cookie. See `stms::secretCookie` in `ssl.hpp`.

    /// If true, allow experimental OpenGL driver features, useful for backwards/forward compatibility.
    constexpr bool enableExperimentalGlew = true;
}


#endif //__STONEMASON_CONFIG_HPP
