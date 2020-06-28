//
// Created by grant on 4/30/20.
//

#pragma once

#ifndef __STONEMASON_NET_SSL_HPP
#define __STONEMASON_NET_SSL_HPP

#include "openssl/ssl.h"
#include <string>
#include <netdb.h>
#include <stms/async.hpp>

#include <sys/socket.h>
#include <sys/poll.h>

namespace stms {
    extern uint8_t secretCookie[secretCookieLen];

    void initOpenSsl();

    void quitOpenSsl();

    int handleSslGetErr(SSL *, int);

    unsigned long handleSSLError();

    inline void flushSSLErrors() {
        while (handleSSLError() != 0);
    }

    std::string getAddrStr(const sockaddr *const addr);

    enum FDEventType {
        eWriteReady = POLLOUT,
        eReadReady = POLLIN
    };

    class _stms_SSLBase {
    protected:
        bool isServ{};
        bool wantV6 = true;
        std::string addrStr;
        addrinfo *pAddrCandidates{};
        addrinfo *pAddr{};
        SSL_CTX *pCtx{};
        int sock = 0;
        unsigned timeoutMs = 1000;
        int maxTimeouts = 9;
        bool running = false;
        stms::ThreadPool *pPool{};
        std::string password{};

        explicit _stms_SSLBase(bool isServ, stms::ThreadPool *pool);

        virtual void onStart();
        virtual void onStop();

        bool tryAddr(addrinfo *addr, int num);

        _stms_SSLBase() = default;

        virtual ~_stms_SSLBase();

        // Returns true if ready
        static bool blockUntilReady(int fd, SSL *ssl, short event);

    public:
        void start();

        void stop();

        /**
         * When called on the server side, this function will block until a new client tries to connect (or when
         * the timeout expires, whichever occurs first.).
         *
         * When called on the client side, this function will block until we receive a message from the server.
         * (or when timeout expires, whichever comes first.)
         *
         * @param timeoutMs Maximum amount of time to block for (in milliseconds)
         * @param events Events to wait for (`eReadReady`, `eWriteRead`, or both).
         * @param silent If true, we will skip printing upon a timeout.
         * @return True if we are ready (ie the specified `events` happened), false if we timed out.
         */
        bool waitEvents(int timeoutMs, FDEventType events = eReadReady, bool silent = false) const;

        [[nodiscard]] inline bool isRunning() const {
            return running;
        }

        inline void setTimeout(unsigned timeout) {
            timeoutMs = timeout;
        }

        inline void setMaxIoTimeouts(int newMax) {
            maxTimeouts = newMax;
        }

        inline void enableAntiReplay() {
            SSL_CTX_clear_options(pCtx, SSL_OP_NO_ANTI_REPLAY);
        }

        inline void disableAntiReplay() {
            SSL_CTX_set_options(pCtx, SSL_OP_NO_ANTI_REPLAY);
        }

        void setHostAddr(const std::string &port = "3000", const std::string &addr = "any");

        void setKeyPassword(const std::string &pass);

        void setCertAuth(const std::string &caCert, const std::string &caPath);

        void setPrivateKey(const std::string &key);

        void setPublicCert(const std::string &cert);

        bool verifyKeyMatchCert();

        inline void setIPv6(bool preferV6) {
            wantV6 = preferV6;
        }
    };
}

#endif //__STONEMASON_NET_SSL_HPP
