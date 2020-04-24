//
// Created by grant on 4/22/20.
//

#pragma once

#ifndef __STONEMASON_DTLS_HPP
#define __STONEMASON_DTLS_HPP

#include <string>
#include <thread>
#include <netinet/in.h>
#include <unordered_map>

#include "openssl/ssl.h"
#include "openssl/bio.h"
#include "openssl/err.h"
#include "stms/util.hpp"

namespace stms::net {
    class DTLSServer;

    static void mainThreadWorker(DTLSServer *server);

    static void clientThreadWorker(DTLSServer *server, const std::string &uuid);

    struct BioAddrType {
        sockaddr_storage sockStore;
        sockaddr_in6 sock6;
        sockaddr_in sock4;
    };

    struct DTLSClientRepresentation {
        std::thread thread;
        BioAddrType addr;
        BIO *bio;
        SSL *ssl;
        int sock;
    };

    class DTLSServer {
    private:
        bool IPv4Enabled = true;
        sockaddr_in6 serverAddr{}; // ip46
        SSL_CTX *ctx{};
        int serverSock{};
        bool isRunning = false;
        std::thread controlThread;
        std::unordered_map<std::string, DTLSClientRepresentation> clients;

    public:
        friend void mainThreadWorker(DTLSServer *server);

        friend void clientThreadWorker(DTLSServer *server, const std::string &uuid);

        DTLSServer() = default;

        explicit DTLSServer(const std::string &ipAddr, unsigned short port = 80,
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
