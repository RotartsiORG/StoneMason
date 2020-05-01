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
    unsigned long handleSSLError();

    inline void flushSSLErrors() {
        while (handleSSLError() != 0);
    }

    std::string getAddrStr(sockaddr *addr);

    struct DTLSClientRepresentation {
        uint8_t timeouts = 0;
        std::string addrStr{};
        BIO_ADDR *pBioAddr = nullptr;
        sockaddr *pSockAddr = nullptr;
        size_t inAddrLen{};
        size_t sockAddrLen{};
        BIO *pBio = nullptr;
        SSL *pSsl = nullptr;
        int sock = 0;
        bool doShutdown = true;

        DTLSClientRepresentation() = default;

        virtual ~DTLSClientRepresentation();

        DTLSClientRepresentation &operator=(const DTLSClientRepresentation &rhs) = delete;

        DTLSClientRepresentation(const DTLSClientRepresentation &rhs) = delete;

        DTLSClientRepresentation &operator=(DTLSClientRepresentation &&rhs) noexcept;

        DTLSClientRepresentation(DTLSClientRepresentation &&rhs) noexcept;
    };

    enum SSLCacheModeBits {
        eBoth = SSL_SESS_CACHE_BOTH,
        eClient = SSL_SESS_CACHE_CLIENT,
        eServer = SSL_SESS_CACHE_SERVER,
        eNoAutoClear = SSL_SESS_CACHE_NO_AUTO_CLEAR,
        eNoInternal = SSL_SESS_CACHE_NO_INTERNAL,
        eNoInternalLookup = SSL_SESS_CACHE_NO_INTERNAL_LOOKUP,
        eNoInternalStore = SSL_SESS_CACHE_NO_INTERNAL_STORE
    };

    class DTLSServer {
    private:;
        bool wantV6{};
        std::string addrStr;
        addrinfo *pAddrCandidates{};
        addrinfo *pAddr{};
        SSL_CTX *pCtx{};
        int serverSock = 0;
        timeval timeout{};
        bool isRunning = false;
        stms::ThreadPool *pPool{};
        std::unordered_map<std::string, DTLSClientRepresentation> clients;
        char *password{};

        // When called, int is the len, which is ALWAYS >0. uint8_t is an array of bytes with that length.
        // The string is client uuid, sockaddr is client address
        std::function<void(const std::string &, const sockaddr *const, uint8_t *, int)> recvCallback = [](
                const std::string &, const sockaddr *const, uint8_t *, int) {};
        std::function<void(const std::string &, const sockaddr *const)> connectCallback = [](const std::string &,
                                                                                             const sockaddr *const) {};
        std::function<void(const std::string &, const sockaddr *const)> disconnectCallback = [](const std::string &,
                                                                                                const sockaddr *const) {};

        bool tryAddr(addrinfo *addr, int num);

        static int handleSslGetErr(SSL *, int);

    public:

        DTLSServer() = default;

        /**
         * @fn stms::net::DTLSServer::DTLSServer(stms::ThreadPool *pool, const std::string &addr = "any",
         *                  const std::string &port = "3000", bool preferV6 = true,
         *                  const std::string &certPem = "server-cert.pem",
         *                  const std::string &keyPem = "server-key.pem",
         *                  const std::string &caCert = "", const std::string &caPath = "",
         *                  const std::string &password = "");
         * @brief Initialize a `DTLSServer`. No connection is opened.
         *
         * @param pool The thread pool for submitting async tasks to. Currently unused.
         * @param addr String containing IPv4 (`127.0.0.1`), IPv6 (`2001:db8:85a3::8a2e:370:7334`), a
         *             hostname to be resolved (`example.com`) or the value `any`.
         *             If `any` is used, the address of the host machine will be used. (Like `INADDR_ANY`)
         *             The server will be hosted on this address.
         * @param port String containing either a port number (`3000`), or a service to be resolved (`http` or `ftp`).
         * @param preferV6 If true, IPv6 addresses would be used instead of IPv4. Otherwise, IPv4 is used.
         * @param certPem Path to the certificate pem file to be used as the server certificate.
         * @param keyPem Path to the private key pem file to be used as the server private key.
         * @param caCert Path to the Certificate Authority certificate pem file to be used. Will be ignored if empty.
         * @param caPath The directory in which to look for Certificate Authority certificate pem files.
         *                Will be ignored if empty.
         * @param password Password for the private pem file. (Only applicable for password-protected private keys).
         *                 Will be ignored if empty.
         */
        explicit DTLSServer(stms::ThreadPool *pool, const std::string &addr = "any",
                            const std::string &port = "3000", bool preferV6 = true,
                            const std::string &certPem = "server-cert.pem",
                            const std::string &keyPem = "server-key.pem",
                            const std::string &caCert = "", const std::string &caPath = "",
                            const std::string &password = ""
        );

        virtual ~DTLSServer();

        DTLSServer &operator=(const DTLSServer &rhs) = delete;

        DTLSServer(const DTLSServer &rhs) = delete;

        DTLSServer &operator=(DTLSServer &&rhs) noexcept;

        DTLSServer(DTLSServer &&rhs) noexcept;

        inline void
        setRecvCallback(const std::function<void(const std::string &, const sockaddr *const, uint8_t *, int)> &newCb) {
            recvCallback = newCb;
        }

        inline void setConnectCallback(const std::function<void(const std::string &,
                                                                const sockaddr *const)> &newCb) {
            connectCallback = newCb;
        }

        inline void setDisconnectCallback(const std::function<void(const std::string &,
                                                                   const sockaddr *const)> &newCb) {
            disconnectCallback = newCb;
        }

        // Try to send a message. A return >0 is success, =0 is invalid clientUUID, <0 is openssl error.
        /**
         * @fn int stms::net::DTLSServer::send(const std::string &clientUuid, char *msg, std::size_t msgLen)
         * @brief Send an message (in the form of an array of `uint8_t`s) to a client.
         *
         * @param clientUuid UUIDv4 of the client to send this message to.
         * @param msg Array of unsigned chars to send to the client
         * @param msgLen Length of `msg` in bytes (octets)
         * @return If `clientUuid` is invalid, 0 is returned. If there is an OpenSSL error, a value < 0 is returned.
         *         If `-1`, `-5`, or `-999` is returned, there was a fatal error and the connection to the client
         *         has been closed. If `-3` or `-2` is returned, you can retry immediately.
         *         If `-6` is returned, the client has closed their writing connection, so
         *         do not attempt to read more data. If another negative value is returned, retry later.
         *         Otherwise, if the operation completed successfully, a positive value containing the number
         *         of bytes sent would be returned.
         */
        int send(const std::string &clientUuid, const uint8_t *msg, int msgLen);

        /**
         * @fn void stms::net::DTLSServer::start()
         * @brief Starts the server. Opens a connection.
         */
        void start();

        /**
         * @fn bool stms::net::DTLSServer::tick()
         * @brief Tick the server, to be called at regular intervals.
         *
         * `tick()` accepts incoming client connections and
         * loops through the list of clients and calls the receive callback if any data is available.
         *
         * @return True if the server is running and ticking succeeded, otherwise false.
         */
        bool tick();

        /**
         * @fn void stms::net::DTLSServer::stop()
         * @brief Stop the server. Kill the connection.
         */
        void stop();

        inline void setCacheMode(SSLCacheModeBits flags) {
            SSL_CTX_set_session_cache_mode(pCtx, flags);
        }
    };
}


#endif //__STONEMASON_DTLS_HPP
