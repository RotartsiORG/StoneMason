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

#include "stms/net/plain.hpp"

namespace stms {

    /// Internal non-fatal ssl exception. Implementation detail. If this is thrown but not caught, it's a bug
    class SSLException : public std::exception {};
    /// Internal non-fatal ssl exception. Implementation detail. If this is thrown but not caught, it's a bug
    class SSLWantWriteException : public std::exception {};
    /// Internal non-fatal ssl exception. Implementation detail. If this is thrown but not caught, it's a bug
    class SSLWantReadException : public std::exception {};
    /// Internal fatal ssl exception. Implementation detail. If this is thrown but not caught, it's a bug
    class SSLFatalException : public std::exception {};

    /// @brief This is `secretCookieLen` of random data to be used for DTLS stateless cookie exchanges
    ///        (for protection against DDoS attacks).
    inline uint8_t *&getSecretCookie() {
        static uint8_t val[secretCookieLen];
        static auto *ret = reinterpret_cast<uint8_t *>(val);
        return ret;
    }

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
    class _stms_SSLBase : public _stms_PlainBase {
    protected:

        SSL_CTX *pCtx{}; //!< Internal implementation detail of OpenSSL context. See OpenSSL docs for `SSL_CTX`

        /// Password for the private key file (see `setPrivateKey`), if it is password-protected.
        std::string password{};

        /**
         * @brief Internal implementation detail. Don't touch this.
         * @param isServ See documentation for member `isServ`
         * @param pool See documentation for member `pPool`
         * @param isUdp See documentatino for member `isUdp`.
         */
        explicit _stms_SSLBase(bool isServ, stms::PoolLike *pool, bool isUdp);

        void moveSslBase(_stms_SSLBase *rhs);

        _stms_SSLBase() = default;
        ~_stms_SSLBase() override; //!< virtual destructor

        /**
         * @brief Simple wrapper around `poll`. Internal implementation detail. Don't touch. See man for `poll`.
         * @param fd Socket file descriptor to listen on
         * @param ssl OpenSSL `SSL` object
         * @param event Event to listen for. `POLLIN` or `POLLOUT`.
         * @return True if `event` happened, false if we timed out.
         */
        bool blockUntilReady(int fd, SSL *ssl, short event) const;

        _stms_SSLBase &operator=(_stms_SSLBase &&rhs) noexcept; //!< DUH
        _stms_SSLBase(_stms_SSLBase &&rhs) noexcept; //!< Duh

    public:
        _stms_SSLBase &operator=(const _stms_SSLBase &rhs) = delete; //!< Deleted copy assignment operator=
        _stms_SSLBase(const _stms_SSLBase &rhs) = delete; //!< Deleted copy constructor

        /**
         * @brief Set the TLS/DTLS session caching mode. See OpenSSL docs for `SSL_CTX_set_session_cache_mode`.
         * @param flags Any combination of `SSLCacheModeBits` (combined using bitwise OR)
         */
        inline void setCacheMode(SSLCacheModeBits flags) {
            SSL_CTX_set_session_cache_mode(pCtx, flags);
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
         * @brief Set the default trust stores. See SSL docs for `SSL_CTX_set_default_verify_paths`
         */
        void trustDefault();

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

        void setVerifyMode(int flags);
    };
}

#endif //__STONEMASON_NET_SSL_HPP
