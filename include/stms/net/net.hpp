/**
 * @file stms/net/net.hpp
 * @author Grant Yang (rotartsi0482@gmail.com)
 * @brief Provides common networking functionality for everyting in the stms/net directory.
 *        You should not have to include this manually.
 * @date 2021-03-28
 */

#pragma once

#ifndef __STONEMASON_NET_NET_HPP
#define __STONEMASON_NET_NET_HPP
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

    /**
     * @brief Base class for all servers and clients, both plain and (D)TLS.
     */
    class _stms_PlainBase {
    protected:
        bool isServ{}; //!< Determines if this is a server or client. Do not alter after construction.
        bool isUdp{}; //!< If true, UDP is used. Otherwise, TCP. Do not alter after construction.

        bool wantV6 = true; //!< Controls if IPv6 addresses are chosen over IPv4 or vice versa (from addr returned from `getaddrinfo`)

        std::string addrStr; //!< String representation of the ip address of the server.
        addrinfo *pAddrCandidates{}; //!< Raw struct returned from `getaddrinfo`. See `man` for more info.

        addrinfo *pAddr{}; //!< Selected address from `pAddrCandidates` to use.

        int sock = 0; //!< Socket file descriptor.

        unsigned timeoutMs = 15000; //!< Number of milliseconds to wait for IO before kicking peer.
        int maxTimeouts = 9; //!< Maximum number of times an IO operation can time out before giving up.

        bool running = false; //!< Bool of if the server/client is online.

        stms::PoolLike *pPool{}; //!< Pool to submit tasks to.

        explicit _stms_PlainBase(bool isServ, stms::PoolLike *pool, bool isUdp); //!< Internal constructor.

        virtual void onStart() {}; //!< Internal overridable handler. Don't touch
        virtual void onStop() {}; //!< Internal overridable handler. Don't touch

        bool tryAddr(addrinfo *addr, int num); //!< Attempt to use an address from `pAddrCandidates`. For internal implementation.

        _stms_PlainBase() = default; //!< Protected default constructor
        virtual ~_stms_PlainBase(); //!< Virtual destructor

        void movePlain(_stms_PlainBase *rhs); //!< Internal handler for subclasses to move super class.

        _stms_PlainBase &operator=(_stms_PlainBase &&rhs) noexcept; //!< Protected move assignment operator
        _stms_PlainBase(_stms_PlainBase &&rhs) noexcept; //!< Protected move constructor.

    public:
        _stms_PlainBase &operator=(const _stms_PlainBase &rhs) = delete; //!< Deleted copy assignment operator
        _stms_PlainBase(const _stms_PlainBase &rhs) = delete; //!< Deleted copy constructor.

        void start(); //!< Start the server/client, creating the `socket` and `bind`ing or `connect`ing it.

        void stop(); //!< Stop it and `close` the socket.

        /**
         * @brief Block until there are events to process (or until `ioMs` runs out)
         * @param ioMs Maximum number of milliseconds to block.
         */
        virtual void waitEvents(int ioMs) = 0;

        /**
         * @brief Query if the server/client is running. Returns `running`
         * @return true Running and connected
         * @return false Offline.
         */
        [[nodiscard]] inline bool isRunning() const {
            return running;
        }

        /**
         * @brief Set the `pPool` object
         * @param p New pool to submit tasks to
         */
        inline void setPool(PoolLike *p) {
            pPool = p;
        }
        
        /**
         * @brief Set `timeoutMs`
         * @param timeout Number of milliseconds to wait for IO operations before giving up.
         */
        inline void setTimeout(unsigned timeout) {
            timeoutMs = timeout;
        }

        /**
         * @brief Set `maxTimeouts`
         * @param newMax Maximum number of times to retry IO operations before dropping connection.
         */
        inline void setMaxIoTimeouts(int newMax) {
            maxTimeouts = newMax;
        }

        /**
         * @brief Controls what address to bind on (server) or connect to (client). See `man getaddrinfo`
         * @param port Either port number ("80") or service name ("http"). See https://www.iana.org/assignments/service-names-port-numbers/service-names-port-numbers.xhtml
         * @param addr Either host name ("example.com"), IPv4 address ("127.0.0.1"), or IPv6 address ("2001:0db8:85a3:0000:0000:8a2e:0370:7334").
         */
        void setHostAddr(const std::string &port = "3000", const std::string &addr = "any");

        /**
         * @brief Set whether or not IPv6 or IPv4 addresses should be used 
         * @param preferV6 If true, IPv6 addresses are used. If false, IPv4.
         */
        inline void setIPv6(bool preferV6) {
            wantV6 = preferV6;
        }

        /**
         * @brief Get `timeoutMs`
         * @return unsigned Number of milliseconds to wait for IO operations before giving up.
         */
        [[nodiscard]] inline unsigned getTimeout() const {
            return timeoutMs;
        }
    };
}

#endif //__STONEMASON_NET_SSL_HPP
