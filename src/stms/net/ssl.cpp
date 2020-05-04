//
// Created by grant on 4/30/20.
//

#include <netinet/in.h>
#include <arpa/inet.h>
#include "stms/net/ssl.hpp"
#include "stms/logging.hpp"
#include "openssl/rand.h"
#include "openssl/err.h"

namespace stms::net {
    std::string getAddrStr(const sockaddr *const addr) {
        if (addr->sa_family == AF_INET) {
            auto *v4Addr = reinterpret_cast<const sockaddr_in *const>(addr);
            char ipStr[INET_ADDRSTRLEN];
            inet_ntop(addr->sa_family, &(v4Addr->sin_addr), ipStr, INET_ADDRSTRLEN);
            return std::string(ipStr) + ":" + std::to_string(ntohs(v4Addr->sin_port));
        } else if (addr->sa_family == AF_INET6) {
            auto *v6Addr = reinterpret_cast<const sockaddr_in6 *const>(addr);
            char ipStr[INET6_ADDRSTRLEN];
            inet_ntop(addr->sa_family, &(v6Addr->sin6_addr), ipStr, INET6_ADDRSTRLEN);
            return std::string(ipStr) + ":" + std::to_string(ntohs(v6Addr->sin6_port));
        }
        return "[ERR: BAD FAM]";
    }

    unsigned long handleSSLError() {
        unsigned long ret = ERR_get_error();
        if (ret != 0) {
            char errStr[256]; // Min length specified by man pages for ERR_error_string_n()
            ERR_error_string_n(ret, errStr, 256);
            STMS_WARN("[** OPENSSL ERROR **]: {}", errStr);
        }
        return ret;
    }

    int handleSslGetErr(SSL *ssl, int ret) {
        switch (SSL_get_error(ssl, ret)) {
            case SSL_ERROR_NONE: {
                return ret;
            }
            case SSL_ERROR_ZERO_RETURN: {
                STMS_WARN("Tried to preform SSL IO, but peer has closed the connection! Don't try to read more data!");
                flushSSLErrors();
                return -6;
            }
            case SSL_ERROR_WANT_WRITE: {
                STMS_WARN("Unable to complete OpenSSL call: Want Write. Please retry. (Auto Retry is on!)");
                return -3;
            }
            case SSL_ERROR_WANT_READ: {
                STMS_WARN("Unable to complete OpenSSL call: Want Read. Please retry. (Auto Retry is on!)");
                return -2;
            }
            case SSL_ERROR_SYSCALL: {
                STMS_WARN("System Error from OpenSSL call: {}", strerror(errno));
                flushSSLErrors();
                return -5;
            }
            case SSL_ERROR_SSL: {
                STMS_WARN("Fatal OpenSSL error occurred!");
                flushSSLErrors();
                return -1;
            }
            case SSL_ERROR_WANT_CONNECT: {
                STMS_WARN("Unable to complete OpenSSL call: SSL hasn't been connected! Retry later!");
                return -7;
            }
            case SSL_ERROR_WANT_ACCEPT: {
                STMS_WARN("Unable to complete OpenSSL call: SSL hasn't been accepted! Retry later!");
                return -8;
            }
            case SSL_ERROR_WANT_X509_LOOKUP: {
                STMS_WARN("The X509 Lookup callback asked to be recalled! Retry later!");
                return -4;
            }
            case SSL_ERROR_WANT_ASYNC: {
                STMS_WARN("Cannot complete OpenSSL call: Async operation in progress. Retry later!");
                return -9;
            }
            case SSL_ERROR_WANT_ASYNC_JOB: {
                STMS_WARN("Cannot complete OpenSSL call: Async thread pool is overloaded! Retry later!");
                return -10;
            }
            case SSL_ERROR_WANT_CLIENT_HELLO_CB: {
                STMS_WARN("ClientHello callback asked to be recalled! Retry Later!");
                return -11;
            }
            default: {
                STMS_WARN("Got an Undefined error from `SSL_get_error()`! This should be impossible!");
                return -999;
            }
        }
    }

    void initOpenSsl() {
        // OpenSSL initialization
        if (OpenSSL_version_num() != OPENSSL_VERSION_NUMBER) {
            STMS_CRITICAL("[* FATAL ERROR *] OpenSSL version mismatch! "
                          "Linked and compiled/included OpenSSL versions are not the same!");
            STMS_CRITICAL("Compiled/Included OpenSSL {} while linked OpenSSL {}",
                          OPENSSL_VERSION_TEXT, OpenSSL_version(OPENSSL_VERSION));
            if (OpenSSL_version_num() >> 20 != OPENSSL_VERSION_NUMBER >> 20) {
                STMS_CRITICAL("OpenSSL major and minor version numbers do not match. Aborting.");
                // TODO: implement
                STMS_CRITICAL("Set `STMS_IGNORE_SSL_MISMATCH` to `true` in `config.hpp` to ignore this error.");
                STMS_CRITICAL("Only do this if you plan on NEVER using OpenSSL functionality.");
                exit(1);
            }
        }

        if (OPENSSL_VERSION_NUMBER < 0x1010102fL) {
            STMS_CRITICAL("{} is outdated and insecure! At least OpenSSL 1.1.1a is required!",
                          OpenSSL_version(OPENSSL_VERSION));
            // TODO: implement
            STMS_CRITICAL("Set `STMS_IGNORE_OLD_SSL` to `true` in `config.hpp` to ignore this error.");
            STMS_CRITICAL("Only do this if you plan on NEVER using OpenSSL functionality.");
            exit(1);
        }

        int pollStatus = RAND_poll();
        if (pollStatus != 1 || RAND_status() != 1) {
            STMS_CRITICAL("[* FATAL ERROR *] Unable to seed OpenSSL RNG with enough random data!");
            STMS_CRITICAL("OpenSSL may generate insecure cryptographic keys, and UUID collisions may occur");
            if (!STMS_IGNORE_BAD_RNG) {
                STMS_CRITICAL("Aborting! Set `STMS_IGNORE_BAD_RNG` to ignore this fatal error.");
                STMS_CRITICAL("Only do this if you plan on NEVER using OpenSSL functionality.");
                exit(1);
            } else {
                STMS_CRITICAL("Using insecure RNG! Only do this if you plan on NEVER using OpenSSL functionality.");
                STMS_CRITICAL("Otherwise, set `STMS_IGNORE_BAD_RNG` to `false` in `config.hpp`");
            }
        }

        SSL_COMP_add_compression_method(0, COMP_zlib());
        STMS_INFO("Initialized {}!", OpenSSL_version(OPENSSL_VERSION));
    }
}

