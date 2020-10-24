/**
 * @file stms/net/ssl_client.hpp
 * @brief Provides functionality for DTLS or TLS clients in the form of SSLClient
 * Created by grant on 5/4/20.
 */

#pragma once

#ifndef __STONEMASON_NET_DTLS_CLIENT_HPP
#define __STONEMASON_NET_DTLS_CLIENT_HPP
//!< Include guard

#include <string>
#include <stms/timers.hpp>

#include "stms/async.hpp"
#include "stms/net/ssl.hpp"

#include "openssl/ssl.h"
#include "openssl/bio.h"
#include "openssl/err.h"
#include "openssl/rand.h"
#include "openssl/opensslv.h"

namespace stms {
    /**
     * @brief TLS/TCP or DTLS/UDP client.
     */
    class SSLClient : public _stms_SSLBase {
    private:
        BIO *pBio{}; //!< OpenSSL `BIO` object for this client. Only for DTLS, is `nullptr` on TLS clients.
        SSL *pSsl{}; //!< OpenSSL `SSL` object for this client. Internal impl detail.
        bool doShutdown = false; //!< If true, `SSL_shutdown` is called in `onStop()`.
        bool isReading = false; //!< Flag for if data is currently being read with `SSL_read()`.
        Stopwatch timeoutTimer{}; //!< `Stopwatch` for implementing connection timeouts.

        void onStart() override; //!< Hook called from `start()`. Internal impl detail.
        void onStop() override; //!< Hook called from `stop()`. Internal impl detail

        /**
         * @brief This is a callback function that is called asynchronously when we recieve data from the server.
         *        The first argument (`uint8_t *`) is a pointer to the actual data.
         *        The second argument (`size_t`) is the size, in bytes (octets), of the data pointed to by the 1st arg.
         *
         *        The first argument will ALWAYS be a valid `uint8_t *`, never `nullptr`. The second argument will
         *        ALWAYS be non-zero.
         */
        std::function<void(uint8_t *, size_t)> recvCallback = [](uint8_t *, size_t) {};

        // Connect/Disconnect callback?
    public:

        /**
         * @brief Constructor for `SSLClient`
         * @param pool The `ThreadPool` to submit async tasks to
         * @param isUdp If true, this will use `DTLS/UDP`, otherwise, `TLS/TCP`.
         */
        explicit SSLClient(stms::ThreadPool *pool, bool isUdp);

        /**
         * @brief Set a new `recvCallback`. See documentation for `stms::SSLClient::recvCallback`
         * @param newCb New callback to replace the old one with
         */
        inline void setRecvCallback(const std::function<void(uint8_t *, size_t)> &newCb) {
            recvCallback = newCb;
        }

        /**
         * @brief Send an message (in the form of an array of `uint8_t`s) to the server.
         * @param data Array of unsigned chars to send to the server
         * @param size The length of `data` in bytes (octets)
         * @param copy If true, the contents of `data` are copied. That way, `data` can be destroyed after
         *             passing it into send()`. Otherwise, we read from msg directly and assume it won't be gone.
         * @return A `std::future<int>` is returned that you can use to block until the `SSL_write` operation finishes.
         *         If there is an OpenSSL error, a value < 0 is returned. If a -1 is returned, then send() was called
         *         while the client was stopped, and you must start the client first. If a -2 is returned, then there
         *         was a fatal exception and we disconnected.
         *         If -3 is returned, the operation timed out and we disconnected. Otherwise, a positive integer
         *         containing the number of bytes sent is returned.
         */
        std::future<int> send(const uint8_t *const data, int size, bool copy = false);

        /**
         * @brief Block until there is data from the server to read, blocking at maximum `timeoutMs` milliseconds.
         * @param timeoutMs Maximum amount of time to block for in milliseconds
         */
        void waitEvents(int timeoutMs) override;

        ~SSLClient() override; //!< Override of virtual destructor from `_stms_SSLBase`. Internal impl detail.

        SSLClient &operator=(const SSLClient &rhs) = delete; //!< Deleted copy assignment operator=

        SSLClient(const SSLClient &rhs) = delete; //!< Deleted copy constructor

        /**
         * @brief Tick the client asynchronously (i.e. This function will NOT block). This will receive data from the
         *        server and call `stms::SSLClient::recvCallback` asynchronously.
         * @return True if ticking succeeded and the client is running, false if the client has stopped.
         */
        bool tick();

        /**
         * @brief Get the PMTU of this connection. See OpenSSL docs for `DTLS_get_data_mtu`
         * @return The PMTU in bytes, or 0 if the query fails (e.g. if this isn't a DTLS connection)
         */
        inline size_t getMtu() {
            return DTLS_get_data_mtu(pSsl);
        };

        /**
         * @brief UNIMPLEMENTED Move copy operator=
         * @param rhs Right Hand Side of the `std::move`
         * @return A reference to `this`.
         */
        SSLClient &operator=(SSLClient &&rhs) noexcept;

        /**
         * @brief UNIMPLEMENTED Move copy constructor
         * @param rhs Right Hand Side of the `std::move`
         */
        SSLClient(SSLClient &&rhs) noexcept;
    };
}


#endif //__STONEMASON_NET_DTLS_CLIENT_HPP
