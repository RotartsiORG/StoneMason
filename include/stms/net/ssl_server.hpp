/**
 * @file stms/net/ssl_server.hpp
 * @brief Provides functionality for DTLS or TLS servers in the form of `SSLServer`
 * Created by grant on 4/22/20.
 */

#pragma once

#ifndef __STONEMASON_NET_DTLS_SERVER_HPP
#define __STONEMASON_NET_DTLS_SERVER_HPP
//!< Include guard

#include <string>
#include <thread>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unordered_map>
#include <stms/async.hpp>
#include <stms/logging.hpp>
#include <stms/util/timers.hpp>

#include "openssl/ssl.h"
#include "openssl/bio.h"
#include "openssl/err.h"
#include "stms/net/ssl.hpp"
#include "stms/util/uuid.hpp"

namespace stms {
    class SSLServer;

    /// Struct containing all the client's data. Internal impl detail, don't touch.
    struct ClientRepresentation {
        // TODO: Should a mutex be introduced to sync IO to this? (OpenSSL SSL object especially)
        std::string addrStr{}; //!< Client's address as a ${host}:${port} string
        sockaddr *pSockAddr = nullptr; //!< Client's address. Is a `reinterpret_cast`ed `sockaddr_in` or `sockaddr_in6`
        socklen_t sockAddrLen{}; //!< Size of `pSockAddr`.
        SSL *pSsl = nullptr; //!< OpenSSL `SSL` object
        int sock = 0; //!< Client socket file descriptor
        bool doShutdown = false; //!< If true, `SSL_shutdown` is called on `pSsl` when this object is destroyed.
        bool isReading = false; //!< Flag for if a `SSL_read` is in progress.

        /// A `Stopwatch` for checking if the connection timed out (ie `timeoutMs` milliseconds passed without response)
        stms::Stopwatch timeoutTimer;

        /// Struct containing the parts of a `ClientRepresentation` relevant for DTLS connections
        struct DTLSSpecific {
            BIO_ADDR *pBioAddr = nullptr; //!< `pSockAddr` represented as a OpenSSL `BIO_ADDR`
            BIO *pBio = nullptr; //!< OpenSSL `BIO` object bound to this client
        };

        /// Member containing all DTLS-specific fields. For non-dtls connections, this is always `nullptr`.
        DTLSSpecific *dtls = nullptr;

        SSLServer *serv = nullptr; //!< Pointer to the `SSLServer` that owns this client

        ClientRepresentation() = default; //!< Default constructor

        virtual ~ClientRepresentation(); //!< Virtual destructor: This will block until `SSL_shutdown` is complete

        ClientRepresentation &operator=(const ClientRepresentation &rhs) = delete; //!< Deleted copy assignment operator

        ClientRepresentation(const ClientRepresentation &rhs) = delete; //!< Deleted copy constructor

        /**
         * @brief Move copy operator
         * @param rhs Right Hand Side of the `std::move`
         * @return A reference to `this`
         */
        ClientRepresentation &operator=(ClientRepresentation &&rhs) noexcept;

        /**
         * @brief Move copy constructor
         * @param rhs Right Hand Side of the `std::move`
         */
        ClientRepresentation(ClientRepresentation &&rhs) noexcept;

        /// Terminate the server-side connection to this client. This will block until `SSL_shutdown` completes.
        void shutdownClient();
    };

//    /// Internal impl detail. Don't touch
//    static void handleDtlsConnection(const std::shared_ptr<ClientRepresentation> &cli, SSLServer *voidServ);
//
//    /// Internal impl detail, don't touch
//    static void doHandshake(const std::shared_ptr<ClientRepresentation> &cli, SSLServer *voidServ);

    /// A SSL/TLS server. Can be TCP (TLS) or UDP (DTLS)
    class SSLServer : public _stms_SSLBase {
    private:
        std::unordered_map<UUID, std::shared_ptr<ClientRepresentation>> clients; //!< Table of connected clients.
        std::queue<UUID> deadClients; //!< Queue of clients to be deleted in `tick`
        std::mutex clientsMtx; //!< Mutex for syncing modification of the client table

        /**
         * @brief This is the callback that is called asynchronously for each packet the server receives from a client.
         *        The first argument (`const UUID &`) is the UUID of the client that sent the packet.
         *        The second argument (`const sockaddr *const`) is the address of the client that sent the packet.
         *        The third argument (`uint8_t *`) is a pointer to the actual data that was sent.
         *        The fourth argument (`int`) is the length in bytes of the data sent.
         *
         *        When this function is called, it is guaranteed that the 1st arg is a valid client UUID (unless
         *        the user altered it using `refreshUuid` or `setNewUuid`). The 2nd arg is always a valid `sockaddr *`,
         *        never `nullptr`. The 3rd arg is always a valid `uint8_t *`, never `nullptr`. The 4th arg is always
         *        a positive `int` that is greater than 0.
         */
        std::function<void(const UUID &, const sockaddr *const, uint8_t *, int)> recvCallback = [](
                const UUID &, const sockaddr *const, uint8_t *, int) {};

        /**
         * @brief This is the callback that is called asynchronously for each client that connects to the server.
         *        The first argument (`const UUID &`) is the UUID of the newly connected client.
         *        The second argument (`const sockaddr *const`) is the address of the newly connected client.
         *
         *        When this function is called, it is guaranteed that the 1st arg is a valid client UUID (unless
         *        the user altered it using `refreshUuid` or `setNewUuid`). The 2nd arg is always a valid `sockaddr *`,
         *        never `nullptr`.
         */
        std::function<void(const UUID &, const sockaddr *const)> connectCallback = [](const UUID &,
                                                                                             const sockaddr *const) {};


        /**
         * @brief This is the callback function that is called asynchronously when a client disconnects from the server.
         *        The first argument (`const UUID &`) is the UUID of the client that disconnected.
         *        The second argument (`const sockaddr *const`) is always the address of the client that disconnected
         *
         *        When this function is called, it is guaranteed that the 1st arg is a valid client UUID (unless
         *        the user altered it using `refreshUuid` or `setNewUuid.
         *
         */
        std::function<void(const UUID &, const sockaddr *const)> disconnectCallback = [](const UUID &,
                                                                                              const sockaddr *const) {};

        /// Internal implementation detail. Don't touch
        void handleDtlsConnection(const std::shared_ptr<ClientRepresentation> &cli);

        /// Internal implementation detail. Don't touch
        void doHandshake(const std::shared_ptr<ClientRepresentation> &cli);

        friend struct ClientRepresentation; //!< Internal implementation detail. Don't touch

        void onStop() override; //!< Hook called in `stop`. Internal impl detail.

    public:

        /**
         * @brief Constructor for creating a new `SSLServer` & initialize all settings to default.
         * @param pool `ThreadPool` to submit async tasks to.
         * @param isUdp If true, UDP/DTLS will be used. Otherwise, TCP/TLS is used.
         */
        explicit SSLServer(stms::PoolLike *pool, bool isUdp);

        ~SSLServer(); //!< Override of virtual destructor of `_stms_SSLBase`.

        SSLServer &operator=(const SSLServer &rhs) = delete; //!< Deleted copy operator=

        SSLServer(const SSLServer &rhs) = delete; //!< Deleted copy constructor

        /**
         * @brief UNIMPLEMENTED move copy operator=
         * @param rhs Right Hand Side of the `std::move`
         * @return A reference to this instance.
         */
        SSLServer &operator=(SSLServer &&rhs) noexcept;

        /**
         * @brief UNIMPLEMENTED move copy constructor
         * @param rhs Right Hand Side of this `std::move`
         */
        SSLServer(SSLServer &&rhs) noexcept;

        /**
         * @brief Set the new `recvCallback`. See documentation for `stms::SSLServer::recvCallback`
         * @param newCb The new callback to replace the old one
         */
        inline void
        setRecvCallback(const std::function<void(const UUID &, const sockaddr *const, uint8_t *, int)> &newCb) {
            recvCallback = newCb;
        }

        /**
         * @brief Set the new `disconnectCallback`. See documentation for `stms::SSLServer::disconnectCallback`
         * @param newCb The new callback to replace the old one
         */
        inline void setConnectCallback(const std::function<void(const UUID &,
                                                                const sockaddr *const)> &newCb) {
            connectCallback = newCb;
        }

        /**
         * @brief Set the new `connectCallback`. See documentation for `stms::SSLServer::connectCallback`
         * @param newCb The new callback to replace the old one
         */
        inline void setDisconnectCallback(const std::function<void(const UUID &,
                                                                   const sockaddr *const)> &newCb) {
            disconnectCallback = newCb;
        }

        /**
         * @brief Replace the client's UUID with a newly generated UUIDv4
         * @param client Client UUID to replace
         * @return Newly generated UUID that it was replaced with. **NOTE: If no client with the uuid `client` exists,
         *         a UUID filled with 0s is returned
         */
        UUID refreshUuid(const UUID &client);

        /**
         * @brief Replace a client's uuid with a new one.
         * @param old Old UUID to replace
         * @param newUuid New UUID to replace it with.
         * @return True if a client had uuid `old` and the uuid was updated. False of no client with UUID `old` exists.
         */
        bool setNewUuid(const UUID &old, const UUID &newUuid);

        /**
         * @brief Generate a list of all the currently connected clients. O(n).
         * @return A `std::vector` containing all the uuids of the currently connected clients.
         */
        std::vector<UUID> getClientUuids();

        /**
         * @brief Get the number of currently connected clients
         * @return Number of clients
         */
        inline std::size_t getNumClients() {
            std::lock_guard<std::mutex> lg(clientsMtx);
            return clients.size();
        };

        /**
         * @brief Block until there is data to be read, will block at maximum `timeoutMs` milliseconds.
         * @param timeoutMs Maximum amount of time to block for, in milliseconds
         */
        void waitEvents(int timeoutMs) override;

        /**
         * @brief Block until there is data to be read from the specified clients, or there is an incoming connection.
         * @param toWait A list of clients we poll. If any packet arrives from these clients, this function unblocks.
         * @param unblockForIncoming If true, this function unblocks when a new client tries to connect to the server.
         * @param to Maximum amount of time to block for, in milliseconds
         */
        void waitEventsFrom(const std::vector<UUID> &toWait, bool unblockForIncoming, int to);

        /**
         * @brief Disconnect ('kick') the client with the following uuid. **Note: this function will NOT block, as the
         *        client isn't disconnected immediately, but queued to be disconnected on the next call to `tick`.
         *        As a result, the next call to `tick` WILL block until `SSL_shutdown` completes.**
         * @param cliId The uuid of the client to kick
         */
        void kickClient(const UUID &cliId);

        /**
         * @brief Send an message (in the form of an array of `uint8_t`s) to a client.
         * @param clientUuid UUID of the client to send this message to.
         * @param msg Array of unsigned chars to send to the client
         * @param msgLen Length of `msg` in bytes (octets)
         * @param cpy If true, the contents of `msg` are copied. That way, `msg` can be destroyed after passing it into
         *            `send()`. Otherwise, we read from msg directly and assume it won't be gone.
         * @return A `std::future<int>` is returned that you can use to block until the `SSL_write` operation finishes.
         *         If `clientUuid` is invalid, 0 is returned. If the server was stopped, -1 is returned; you must
         *         first start the server before calling `send()`. If there was a fatal SSL error, -2 is returned and
         *         the client is kicked. If the operation timed out, -3 is returned and the client is kicked.
         *         Otherwise, the number of bytes sent is returned.
         */
        std::future<int> send(const UUID &clientUuid, const uint8_t *const msg, int msgLen, bool cpy = false);

        /**
         * @brief Accept incoming client connections & receive data asynchronously. To be called at a regular interval.
         *        Usually, this function will not block. However, if a client disconnects, it will block until
         *        `SSL_shutdown` completes.
         * @return True if the server is running and ticking succeeded, otherwise false.
         */
        bool tick();

        /**
         * @brief Gets the PMTU of the connection the a specific client. See OpenSSL docs for `DTLS_get_data_mtu`
         *        **ONLY RELEVANT ON DTLS CONNECTIONS**
         *
         * @param cli DTLS client to query the PMTU of
         * @return The PMTU in bytes, or 0 if `cli` doesn't exist or this isn't a DTLS connection (or otherwise fails).
         */
        size_t getMtu(const UUID &cli);
    };
}


#endif //__STONEMASON_NET_DTLS_SERVER_HPP
