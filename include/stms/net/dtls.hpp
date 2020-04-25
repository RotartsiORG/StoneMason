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
    class DTLSServer;

    static void mainThreadWorker(DTLSServer *server);

    static void clientThreadWorker(DTLSServer *server, const std::string &uuid);

    unsigned long handleSSLError();

    inline void flushSSLErrors() {
        while (handleSSLError() != 0);
    }

    struct DTLSClientRepresentation {
        BIO_ADDR *addr;
        BIO *bio;
        SSL *ssl;
        int sock;
    };

    class DTLSServer {
    private:
        bool v6 = false;
        addrinfo servAddr{};
        std::string servAddrStr;
        SSL_CTX *ctx{};
        int serverSock{};
        bool isRunning = false;
        stms::ThreadPool *pool{};

    public:
        friend void mainThreadWorker(DTLSServer *server);

        friend void clientThreadWorker(DTLSServer *server, const std::string &uuid);

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
