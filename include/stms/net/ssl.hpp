/**
 * @file stms/net/ssl.hpp
 * @brief This file provides basic functionality of OpenSSL server/client.
 *        Normally, you wouldn't have to touch this file. It is included by `stms/net/ssl_client.hpp` and
 *        `stms/net/ssl_server.hpp`. Including this file becomes redundant.
 * Created by Grant Yang on 4/30/20.
 */

#pragma once

#ifndef __STONEMASON_NET_SSL_HPP
#define __STONEMASON_NET_SSL_HPP
//!< Include guard

#include "openssl/ssl.h"
#include <string>
#include <netdb.h>
#include <stms/async.hpp>

#include <sys/socket.h>
#include <sys/poll.h>

#include "stms/except.hpp"

namespace stms {

    /// Internal non-fatal ssl exception. Implementation detail. If this is thrown but not caught, it's a bug
    class SSLException : public InternalException {};
    /// Internal non-fatal ssl exception. Implementation detail. If this is thrown but not caught, it's a bug
    class SSLWantWriteException : public InternalException {};
    /// Internal non-fatal ssl exception. Implementation detail. If this is thrown but not caught, it's a bug
    class SSLWantReadException : public InternalException {};
    /// Internal fatal ssl exception. Implementation detail. If this is thrown but not caught, it's a bug
    class SSLFatalException : public InternalException {};

    /// @brief This is `secretCookieLen` of random data to be used for DTLS stateless cookie exchanges
    ///        (for protection against DDoS attacks).
    extern uint8_t secretCookie[secretCookieLen];

    void initOpenSsl(); //!< Init OpenSSL parts of STMS. Use `stms::initAll` if you wish to init everything.

    void quitOpenSsl(); //!< Quit OpenSSL parts of STMS. No need to call this if you used `stms::initAll`.

    int handleSslGetErr(SSL *, int); //!< Internal impl detail. You shouldn't touch this.

    /**
     * @brief Log a single OpenSSL error, if there are errors to be processed.
     * @return The OpenSSL error code returned by `ERR_get_error`. See OpenSSL documentation.
     */
    unsigned long handleSSLError();

    /// Calls `handleSSLError` in a loop until there are no more errors to process.
    inline void flushSSLErrors() {
        while (handleSSLError() != 0);
    }

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

    /// Simple wrapper around OpenSSL server cache modes (see OpenSSL docs for `SSL_CTX_set_session_cache_mode`)
    enum SSLCacheModeBits {
        eOff = SSL_SESS_CACHE_OFF, //!< No session caching takes place. (See OpenSSL docs for `SSL_SESS_CACHE_OFF`)
        eBoth = SSL_SESS_CACHE_BOTH, //!< Equal to `eClient | eServer`. (See OpenSSL docs for `SSL_SESS_CACHE_BOTH`)
        eClient = SSL_SESS_CACHE_CLIENT, //!< Cache client sessions. (See OpenSSL docs for `SSL_SESS_CACHE_CLIENT`)
        eServer = SSL_SESS_CACHE_SERVER, //!< Cache server sessions. This is the default. See `SSL_SESS_CACHE_SERVER`
        eNoAutoClear = SSL_SESS_CACHE_NO_AUTO_CLEAR, //!< Disable auto clearing of expired sessions. See `SSL_SESS_CACHE_NO_AUTO_CLEAR`
        eNoInternal = SSL_SESS_CACHE_NO_INTERNAL, //!< Equal to `eNoInternalLookup | eNoInternalStore`. See `SSL_SESS_CACHE_NO_INTERNAL`
        eNoInternalLookup = SSL_SESS_CACHE_NO_INTERNAL_LOOKUP, //!< Don't internally look up sessions in the cache. See `SSL_SESS_CACHE_NO_INTERNAL_LOOKUP`
        eNoInternalStore = SSL_SESS_CACHE_NO_INTERNAL_STORE //!< Don't store sessions in the cache internally. See `SSL_SESS_CACHE_NO_INTERNAL_STORE`
    };

    /**
     * @brief Internal implementation detail. You shouldn't have to touch this!
     *        `stms::SSLServer` and `stms::SSLClient` inherit from this class.
     */
    class _stms_SSLBase {
    protected:
        bool isServ{}; //!< True if this instance is acting as a server, false if it's a client.
        bool isUdp{}; //!< True if this instance is using UDP/DTLS, false if it's using TCP/TLS

        bool wantV6 = true; //!< If true, this instance will prefer IPv6 over IPv4, the opposite if false.

        std::string addrStr; //!< String repr of `pAddr`. See `getAddrStr`.
        /// linked list of possible candidates for addresses. See docs for `pAddr` and `getaddrinfo`
        addrinfo *pAddrCandidates{};

        /**
         * @brief An address from `pAddrCandidates` that we selected. This param is set on the first call to `start`.
         *        If this instance is a server, this is the address we are bound on (i.e. `bind` was called with this),
         *        otherwise, this is the address of the server we are trying to connect to as a client.
         */
        addrinfo *pAddr{};

        SSL_CTX *pCtx{}; //!< Internal implementation detail of OpenSSL context. See OpenSSL docs for `SSL_CTX`
        int sock = 0; //!< File descriptor for our socket. Internal implementation detail. See man pages for `socket`.

        /**
         * The timeout (in milliseconds) for IO operations like `send` and reading.
         * This is the maximum amount of time we will wait for an IO operation to complete.
         * This timeout is waited for on a thread pool thread, so the main thread won't be affected by this.
         */
        unsigned timeoutMs = 1000;
        int maxTimeouts = 9; //!< The maximum number of times we are allowed to retry an IO operation like `send` & read

        bool running = false; //!< Flag for if the server/client is running (DUH).

        /// The `ThreadPool` to submit async tasks to. This is the `ThreadPool` passed into the constructor.
        stms::ThreadPool *pPool{};

        /// Password for the private key file (see `setPrivateKey`), if it is password-protected.
        std::string password{};

        /**
         * @brief Internal implementation detail. Don't touch this.
         * @param isServ See documentation for member `isServ`
         * @param pool See documentation for member `pPool`
         * @param isUdp See documentatino for member `isUdp`.
         */
        explicit _stms_SSLBase(bool isServ, stms::ThreadPool *pool, bool isUdp);

        virtual void onStart(); //!< Hook called when `start` is called. Internal implementation detail. Don't touch.
        virtual void onStop(); //!< Hook called when `stop` is called. Internal implementation detail. Don't touch.

        /// Try and evaluate a candidate address from `pAddrCandidates`. Internal implementation detail. Don't touch.
        bool tryAddr(addrinfo *addr, int num);

        virtual ~_stms_SSLBase(); //!< virtual destructor

        /**
         * @brief Simple wrapper around `poll`. Internal implementation detail. Don't touch. See man for `poll`.
         * @param fd Socket file descriptor to listen on
         * @param ssl OpenSSL `SSL` object
         * @param event Event to listen for. `POLLIN` or `POLLOUT`.
         * @return True if `event` happened, false if we timed out.
         */
        bool blockUntilReady(int fd, SSL *ssl, short event) const;

    public:
        _stms_SSLBase &operator=(const _stms_SSLBase &rhs) = delete; //!< Deleted copy assignment operator=
        _stms_SSLBase(const _stms_SSLBase &rhs) = delete; //!< Deleted copy constructor

        /**
         * @brief UNIMPLEMENTED Move copy operator
         * @param rhs Right Hand Side of the `std::move`
         * @return A reference to `this`
         */
        _stms_SSLBase &operator=(_stms_SSLBase &&rhs) noexcept;

        /**
         * @brief UNIMPLEMENTED Move copy constructor
         * @param rhs Right Hand Side of the `std::move`
         */
        _stms_SSLBase(_stms_SSLBase &&rhs) noexcept;

        /// Start the server/client!
        void start();

        /// Stop the server/client!
        void stop();

        /**
         * @brief Block until there's data to read, or until the `timeoutMs` runs out, whichever comes first.
         * @param timeoutMs The maximum number of milliseconds to block for.
         */
        virtual void waitEvents(int timeoutMs) = 0;

        /**
         * @brief Query if the server/client is running. See docs for member `running`.
         * @return True if the server/client is running, false otherwise.
         */
        [[nodiscard]] inline bool isRunning() const {
            return running;
        }

        /**
         * @brief Set the maximum number of milliseconds we will wait for IO operations to complete. See docs for
         *        member `timeoutMs`.
         * @param timeout The new value for `timeoutMs`.
         */
        inline void setTimeout(unsigned timeout) {
            timeoutMs = timeout;
        }

        /**
         * @brief Set the TLS/DTLS session caching mode. See OpenSSL docs for `SSL_CTX_set_session_cache_mode`.
         * @param flags Any combination of `SSLCacheModeBits` (combined using bitwise OR)
         */
        inline void setCacheMode(SSLCacheModeBits flags) {
            SSL_CTX_set_session_cache_mode(pCtx, flags);
        }

        /**
         * @brief Set the maximum number of times we retry IO operations. See docs for member `maxTimeouts`.
         * @param newMax The new value for `maxTimeouts`.
         */
        inline void setMaxIoTimeouts(int newMax) {
            maxTimeouts = newMax;
        }

        /// Turn on anti-replay protection. There will be no duplicate packets. Only relevant for DTLS connections.
        inline void enableAntiReplay() {
            SSL_CTX_clear_options(pCtx, SSL_OP_NO_ANTI_REPLAY);
        }

        /// Turn off anti-replay protection. You may receive duplicate packets. Only relevant for DTLS connections.
        inline void disableAntiReplay() {
            SSL_CTX_set_options(pCtx, SSL_OP_NO_ANTI_REPLAY);
        }

        /**
         * @brief If this instance is a server, set the address to host the server on. If it's a client,
         *        set the address to connect to.
         * @param port Port to connect to. Can be "http" or some raw port number "31415". See docs for `getaddrinfo`.
         * @param addr IP to connect to. Can be "www.example.com" or some IP "127.0.0.1". See `getaddrinfo`.
         */
        void setHostAddr(const std::string &port = "3000", const std::string &addr = "any");

        /**
         * @brief Set the password to use for a password-protected private key.
         * @param pass Password of the private key.
         */
        void setKeyPassword(const std::string &pass);

        /**
         * @brief Specify which certificate authority(s) to trust.
         * @param caCert A path to a public certificate of a certificate authority to trust. Can be empty.
         * @param caPath A directory containing the public certificates of the certificate authorities to trust.
         *               Can be empty.
         */
        void setCertAuth(const std::string &caCert, const std::string &caPath);

        /**
         * @brief Specify the path to the private key of the server. (.PEM format)
         * @param key The path to the .pem private key of the server.
         */
        void setPrivateKey(const std::string &key);

        /**
         * @brief Specify the path the the public certificate of the server.
         * @param cert The path to the certificate of the server.
         */
        void setPublicCert(const std::string &cert);

        /**
         * @brief Verify that the private key supplied with `setPrivateKey` and the public cert supplied with
         *        `setPublicCert` match.
         * @return True if the key and cert match, false otherwise.
         */
        bool verifyKeyMatchCert();

        /**
         * @brief Set the preference of IPv6 over IPv4 or IPv4 over IPv6.
         * @param preferV6 If true, IPv6 is preferred over IPv4, otherwise, the opposite.
         */
        inline void setIPv6(bool preferV6) {
            wantV6 = preferV6;
        }

        /**
         * @brief Query the max number of milliseconds we will wait on pending IO operations.
         * @return Number of milliseconds we will wait for IO. See docs for member `timeoutMs`.
         */
        [[nodiscard]] inline unsigned getTimeout() const {
            return timeoutMs;
        }
    };
}

#endif //__STONEMASON_NET_SSL_HPP
