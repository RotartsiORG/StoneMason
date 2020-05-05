//
// Created by grant on 4/30/20.
//

#pragma once

#ifndef __STONEMASON_SSL_HPP
#define __STONEMASON_SSL_HPP

#include "openssl/ssl.h"
#include <string>
#include <netdb.h>
#include <stms/async.hpp>
#include "sys/socket.h"

namespace stms::net {
    void initOpenSsl();

    int handleSslGetErr(SSL *, int);

    unsigned long handleSSLError();

    inline void flushSSLErrors() {
        while (handleSSLError() != 0);
    }

    std::string getAddrStr(const sockaddr *const addr);

    class SSLBase {
    protected:
        bool isServ{};
        bool wantV6{};
        std::string addrStr;
        addrinfo *pAddrCandidates{};
        addrinfo *pAddr{};
        SSL_CTX *pCtx{};
        int sock = 0;
        timeval timeout{};
        unsigned timeoutMs = 1000;
        bool isRunning = false;
        stms::ThreadPool *pPool{};
        char *password{};

        explicit SSLBase(bool isServ, stms::ThreadPool *pool, const std::string &addr = "any",
                         const std::string &port = "3000", bool preferV6 = true,
                         const std::string &certPem = "server-cert.pem",
                         const std::string &keyPem = "server-key.pem",
                         const std::string &caCert = "", const std::string &caPath = "",
                         const std::string &password = ""
        );

        virtual void onStart();

        bool tryAddr(addrinfo *addr, int num);

        SSLBase() = default;

        virtual ~SSLBase();

    public:
        void start();
    };
}

#endif //__STONEMASON_SSL_HPP
