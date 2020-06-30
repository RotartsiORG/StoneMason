//
// Created by grant on 4/22/20.
//

#pragma once

#ifndef __STONEMASON_NET_DTLS_SERVER_HPP
#define __STONEMASON_NET_DTLS_SERVER_HPP

#include <string>
#include <thread>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unordered_map>
#include <stms/async.hpp>
#include <stms/logging.hpp>
#include <stms/timers.hpp>

#include "openssl/ssl.h"
#include "openssl/bio.h"
#include "openssl/err.h"
#include "stms/net/ssl.hpp"
#include "stms/stms.hpp"

namespace stms {
    struct DTLSClientRepresentation {
        uint8_t timeouts = 0;
        std::string addrStr{};
        BIO_ADDR *pBioAddr = nullptr;
        sockaddr *pSockAddr = nullptr;
        socklen_t sockAddrLen{};
        BIO *pBio = nullptr;
        SSL *pSsl = nullptr;
        int sock = 0;
        bool doShutdown = true;
        bool isReading = false;
        stms::Stopwatch timeoutTimer;

        DTLSClientRepresentation() = default;

        virtual ~DTLSClientRepresentation();

        DTLSClientRepresentation &operator=(const DTLSClientRepresentation &rhs) = delete;

        DTLSClientRepresentation(const DTLSClientRepresentation &rhs) = delete;

        DTLSClientRepresentation &operator=(DTLSClientRepresentation &&rhs) noexcept;

        DTLSClientRepresentation(DTLSClientRepresentation &&rhs) noexcept;

        void shutdownClient() const;
    };

    class DTLSServer;

    static void handleClientConnection(const std::shared_ptr<DTLSClientRepresentation> &cli, DTLSServer *voidServ);

    enum SSLCacheModeBits {
        eBoth = SSL_SESS_CACHE_BOTH,
        eClient = SSL_SESS_CACHE_CLIENT,
        eServer = SSL_SESS_CACHE_SERVER,
        eNoAutoClear = SSL_SESS_CACHE_NO_AUTO_CLEAR,
        eNoInternal = SSL_SESS_CACHE_NO_INTERNAL,
        eNoInternalLookup = SSL_SESS_CACHE_NO_INTERNAL_LOOKUP,
        eNoInternalStore = SSL_SESS_CACHE_NO_INTERNAL_STORE
    };

    class DTLSServer : public _stms_SSLBase {
    private:
        std::unordered_map<std::string, std::shared_ptr<DTLSClientRepresentation>> clients;
        std::queue<std::string> deadClients;
        std::mutex clientsMtx;

        // When called, int is the len, which is ALWAYS >0. uint8_t is an array of bytes with that length.
        // The string is client uuid, sockaddr is client address
        std::function<void(const std::string &, const sockaddr *const, uint8_t *, int)> recvCallback = [](
                const std::string &, const sockaddr *const, uint8_t *, int) {};
        std::function<void(const std::string &, const sockaddr *const)> connectCallback = [](const std::string &,
                                                                                             const sockaddr *const) {};
        std::function<void(const std::string &, const std::string &)> disconnectCallback = [](const std::string &,
                                                                                              const std::string &) {};

        friend void handleClientConnection(const std::shared_ptr<DTLSClientRepresentation> &cli, DTLSServer *voidServ);

        friend struct DTLSClientRepresentation;

        void onStop() override;

    public:

        DTLSServer() = default;

        explicit DTLSServer(stms::ThreadPool *pool);

        ~DTLSServer() override;

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
                                                                   const std::string &)> &newCb) {
            disconnectCallback = newCb;
        }

        std::string refreshUuid(const std::string &client);

        bool setNewUuid(const std::string &old, const std::string &newUuid);

        std::vector<std::string> getClientUuids();

        inline std::size_t getNumClients() {
            std::lock_guard<std::mutex> lg(clientsMtx);
            return clients.size();
        };

        void waitEvents(int timeoutMs) override;

        void kickClient(const std::string &cliId);


        // Try to send a message. A return >0 is success, =0 is invalid clientUUID, <0 is openssl error.
        /**
         * @fn int stms::DTLSServer::send(const std::string &clientUuid, char *msg, std::size_t msgLen)
         * @brief Send an message (in the form of an array of `uint8_t`s) to a client.
         *
         * @param clientUuid UUIDv4 of the client to send this message to.
         * @param msg Array of unsigned chars to send to the client
         * @param msgLen Length of `msg` in bytes (octets)
         * @param cpy If true, the contents of msg are copied. That way, msg can be destroyed after passing it into
         *            send. Otherwise, we read from msg directly and assume it won't be gone.
         * @return If `clientUuid` is invalid, 0 is returned. If there is an OpenSSL error, a value < 0 is returned.
         *         If `-1`, `-5`, or `-999` is returned, there was a fatal error and the connection to the client
         *         has been closed. If `-3` or `-2` is returned, you can retry immediately.
         *         If `-6` is returned, the client has closed their writing connection, so
         *         do not attempt to read more data. If another negative value is returned, retry later.
         *         Otherwise, if the operation completed successfully, a positive value containing the number
         *         of bytes sent would be returned.
         */
        std::future<int> send(const std::string &clientUuid, const uint8_t *const msg, int msgLen, bool cpy = false);

        /**
         * @fn bool stms::DTLSServer::tick()
         * @brief Tick the server, to be called at regular intervals.
         *
         * `tick()` accepts incoming client connections and
         * loops through the list of clients and calls the receive callback if any data is available.
         *
         * @return True if the server is running and ticking succeeded, otherwise false.
         */
        bool tick();


        inline void setCacheMode(SSLCacheModeBits flags) {
            SSL_CTX_set_session_cache_mode(pCtx, flags);
        }

        size_t getMtu(const std::string &cli);
    };
}


#endif //__STONEMASON_NET_DTLS_SERVER_HPP
