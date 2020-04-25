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
        std::string addrStr{};
        BIO_ADDR *addr = nullptr;
        sockaddr *sockAddr = nullptr;
        size_t sockAddrLen{};
        BIO *bio = nullptr;
        SSL *ssl = nullptr;
        int sock = 0;

        DTLSClientRepresentation() = default;

        virtual ~DTLSClientRepresentation();

        DTLSClientRepresentation &operator=(const DTLSClientRepresentation &rhs) = delete;

        DTLSClientRepresentation(const DTLSClientRepresentation &rhs) = delete;

        DTLSClientRepresentation &operator=(DTLSClientRepresentation &&rhs) noexcept;

        DTLSClientRepresentation(DTLSClientRepresentation &&rhs) noexcept;
    };

    class DTLSServer {
    private:;
        bool wantV6{};
        std::string addrStr;
        addrinfo *servAddr{};
        addrinfo *activeAddr{};
        SSL_CTX *ctx{};
        int serverSock = 0;
        bool isRunning = false;
        stms::ThreadPool *pool{};
        std::unordered_map<std::string, DTLSClientRepresentation> clients;

        bool tryAddr(addrinfo *addr, int num);

    public:

        DTLSServer() = default;

        explicit DTLSServer(stms::ThreadPool *pool, const std::string &addr = "any", bool preferV6 = true,
                            const std::string &port = "3000",
                            const std::string &certPem = "server-cert.pem",
                            const std::string &keyPem = "server-key.pem");

        virtual ~DTLSServer();

        DTLSServer &operator=(const DTLSServer &rhs) = delete;

        DTLSServer(const DTLSServer &rhs) = delete;

        DTLSServer &operator=(DTLSServer &&rhs) noexcept;

        DTLSServer(DTLSServer &&rhs) noexcept;

        void start();

        bool tick();

        void stop();
    };
}


#endif //__STONEMASON_DTLS_HPP
