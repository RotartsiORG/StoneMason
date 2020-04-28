//
// Created by grant on 4/22/20.
//

#pragma once

#ifndef __STONEMASON_DTLS_HPP
#define __STONEMASON_DTLS_HPP

#include <string>
#include <thread>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unordered_map>
#include <stms/async.hpp>
#include <stms/logging.hpp>

#include "openssl/ssl.h"
#include "openssl/bio.h"
#include "openssl/err.h"
#include "stms/util.hpp"

namespace stms::net {
    unsigned long handleSSLError();

    inline void flushSSLErrors() {
        while (handleSSLError() != 0);
    }

    std::string getAddrStr(sockaddr *addr);

    struct DTLSClientRepresentation {
        uint8_t timeouts = 0;
        std::string addrStr{};
        BIO_ADDR *pBioAddr = nullptr;
        sockaddr *pSockAddr = nullptr;
        size_t sockAddrLen{};
        BIO *pBio = nullptr;
        SSL *pSsl = nullptr;
        int sock = 0;

        DTLSClientRepresentation() = default;

        virtual ~DTLSClientRepresentation();

        DTLSClientRepresentation &operator=(const DTLSClientRepresentation &rhs) = delete;

        DTLSClientRepresentation(const DTLSClientRepresentation &rhs) = delete;

        DTLSClientRepresentation &operator=(DTLSClientRepresentation &&rhs) noexcept;

        DTLSClientRepresentation(DTLSClientRepresentation &&rhs) noexcept;
    };

    enum SSLCacheModeBits {
        eBoth = SSL_SESS_CACHE_BOTH,
        eClient = SSL_SESS_CACHE_CLIENT,
        eServer = SSL_SESS_CACHE_SERVER,
        eNoAutoClear = SSL_SESS_CACHE_NO_AUTO_CLEAR,
        eNoInternal = SSL_SESS_CACHE_NO_INTERNAL,
        eNoInternalLookup = SSL_SESS_CACHE_NO_INTERNAL_LOOKUP,
        eNoInternalStore = SSL_SESS_CACHE_NO_INTERNAL_STORE
    };

    class DTLSServer {
    private:;
        bool wantV6{};
        std::string addrStr;
        addrinfo *pAddrCandidates{};
        addrinfo *pAddr{};
        SSL_CTX *pCtx{};
        int serverSock = 0;
        timeval timeout{};
        bool isRunning = false;
        stms::ThreadPool *pPool{};
        std::unordered_map<std::string, DTLSClientRepresentation> clients;
        char *password{};

        bool tryAddr(addrinfo *addr, int num);

    public:

        DTLSServer() = default;

        explicit DTLSServer(stms::ThreadPool *pool, const std::string &addr = "any",
                            const std::string &port = "3000", bool preferV6 = true,
                            const std::string &certPem = "server-cert.pem",
                            const std::string &keyPem = "server-key.pem",
                            const std::string &caCert = "", const std::string &caPath = "",
                            const std::string &password = ""
        );

        virtual ~DTLSServer();

        DTLSServer &operator=(const DTLSServer &rhs) = delete;

        DTLSServer(const DTLSServer &rhs) = delete;

        DTLSServer &operator=(DTLSServer &&rhs) noexcept;

        DTLSServer(DTLSServer &&rhs) noexcept;

        void start();

        bool tick();

        void stop();

        inline void setCacheMode(SSLCacheModeBits flags) {
            SSL_CTX_set_session_cache_mode(pCtx, flags);
        }
    };
}


#endif //__STONEMASON_DTLS_HPP
