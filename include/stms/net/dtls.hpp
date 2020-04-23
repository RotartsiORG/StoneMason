//
// Created by grant on 4/22/20.
//

#pragma once

#ifndef __STONEMASON_DTLS_HPP
#define __STONEMASON_DTLS_HPP

#include <string>
#include <thread>
#include <netinet/in.h>

#include "openssl/ssl.h"
#include "openssl/bio.h"
#include "openssl/err.h"

namespace stms::net {
    class DTLSServer;

    static void mainThreadWorker(DTLSServer *server);

    struct BioAddrType {
        sockaddr_storage sockStore;
        sockaddr_in6 sock6;
        sockaddr_in sock4;
    };

    class DTLSServer {
    private:
        sockaddr_in6 serverAddr{};
        SSL_CTX *ctx{};
        int serverSock{};
        bool isRunning = false;
        std::thread controlThread;

    public:
        friend void mainThreadWorker(DTLSServer *server);

        DTLSServer() = default;

        explicit DTLSServer(const std::string &ipAddr, unsigned port = 4433,
                            const std::string &certPem = "server-cert.pem",
                            const std::string &keyPem = "server-key.pem");

        virtual ~DTLSServer();

        DTLSServer &operator=(const DTLSServer &rhs) = delete;

        DTLSServer(const DTLSServer &rhs) = delete;

        DTLSServer &operator=(DTLSServer &&rhs) noexcept;

        DTLSServer(DTLSServer &&rhs) noexcept;

        void start();

        void stop();
    };
}


#endif //__STONEMASON_DTLS_HPP
