#pragma once

#ifndef __STONEMASON_NET_PLAIN_HPP
#define __STONEMASON_NET_PLAIN_HPP
//!< Include guard

#include "openssl/ssl.h"
#include <string>
#include <netdb.h>
#include <stms/async.hpp>

#include <sys/socket.h>
#include <sys/poll.h>

namespace stms {

    /**
     * @brief Get the string representation of a `sockaddr *`.
     * @param addr `sockaddr *` to get the string representation of.
     * @return String representation of the `sockaddr *` in the form of `<host>:<port>`.
     */
    std::string getAddrStr(const sockaddr *const addr);

    /// Enum of events to listen for in a `poll()` call. See man pages for `poll`.
    enum FDEventType {
        /// Write ready event. Sending data will no longer block. See man pages for `poll`, under `POLLOUT`.
        eWriteReady = POLLOUT,
        /// Read ready event. Reading data will no longer block. See man pages for `poll`, under `POLLIN`.
        eReadReady = POLLIN
    };

    class _stms_PlainBase {
    protected:
        bool isServ{}; 
        bool isUdp{}; 

        bool wantV6 = true; 

        std::string addrStr; 
        addrinfo *pAddrCandidates{};

        addrinfo *pAddr{};

        int sock = 0; 

        unsigned timeoutMs = 15000;
        int maxTimeouts = 9;

        bool running = false;

        stms::PoolLike *pPool{};

        explicit _stms_PlainBase(bool isServ, stms::PoolLike *pool, bool isUdp);

        virtual void onStart() {};
        virtual void onStop() {};

        bool tryAddr(addrinfo *addr, int num);

        // void internalOpEq(_stms_PlainBase *rhs);

        _stms_PlainBase() = default;
        virtual ~_stms_PlainBase();

        void movePlain(_stms_PlainBase *rhs);

        _stms_PlainBase &operator=(_stms_PlainBase &&rhs) noexcept;
        _stms_PlainBase(_stms_PlainBase &&rhs) noexcept;

    public:
        _stms_PlainBase &operator=(const _stms_PlainBase &rhs) = delete;
        _stms_PlainBase(const _stms_PlainBase &rhs) = delete;

        void start();

        void stop();

        virtual void waitEvents(int timeoutMs) = 0;

        [[nodiscard]] inline bool isRunning() const {
            return running;
        }

        inline void setPool(PoolLike *p) {
            pPool = p;
        }
        
        inline void setTimeout(unsigned timeout) {
            timeoutMs = timeout;
        }

        inline void setMaxIoTimeouts(int newMax) {
            maxTimeouts = newMax;
        }

        void setHostAddr(const std::string &port = "3000", const std::string &addr = "any");


        inline void setIPv6(bool preferV6) {
            wantV6 = preferV6;
        }

        [[nodiscard]] inline unsigned getTimeout() const {
            return timeoutMs;
        }
    };
}

#endif //__STONEMASON_NET_SSL_HPP
