//
// Created by grant on 4/22/20.
//

#include "stms/net/dtls_server.hpp"
#include "stms/stms.hpp"
#include "stms/logging.hpp"

#include <unistd.h>
#include <unordered_map>
#include <sys/socket.h>
#include <arpa/inet.h>

#include <fcntl.h>
#include <poll.h>

#include "openssl/ssl.h"
#include "openssl/rand.h"

namespace stms::net {

    static bool hashSSL(SSL *ssl, unsigned *len, unsigned char *result) {
        sockaddr_storage peer{};

        auto addrSize = BIO_dgram_get_peer(SSL_get_rbio(ssl), &peer);

        *len = DTLS1_COOKIE_LENGTH - 1;
        HMAC(EVP_sha1(), secretCookie, secretCookieLen, (unsigned char *) &peer, addrSize, result, len);
        return true;
    }

    static int genCookie(SSL *ssl, unsigned char *cookie, unsigned int *cookieLen) {
        if (!hashSSL(ssl, cookieLen, cookie)) {
            return 0;
        }

        return 1;
    }

    static int verifyCookie(SSL *ssl, const unsigned char *cookie, unsigned int cookieLen) {
        unsigned expectedLen;
        uint8_t result[DTLS1_COOKIE_LENGTH - 1];
        if (!hashSSL(ssl, &expectedLen, result)) {
            return 0;
        }

        if (cookieLen == expectedLen && std::memcmp(result, cookie, expectedLen) == 0) {
            return 1;
        }

        STMS_WARN("Received an invalid cookie! Are you under a DDoS attack?");
        return 0;

    }

    static void handleClientConnection(const std::shared_ptr<DTLSClientRepresentation> &cli, DTLSServer *serv) {
        int on = 1;

        if (BIO_ADDR_family(cli->pBioAddr) == AF_INET6) {
            auto *v6Addr = new sockaddr_in6();
            v6Addr->sin6_family = AF_INET6;
            v6Addr->sin6_port = BIO_ADDR_rawport(cli->pBioAddr);

            std::size_t inAddrLen = sizeof(in6_addr);
            cli->sockAddrLen = sizeof(sockaddr_in6);
            BIO_ADDR_rawaddress(cli->pBioAddr, &v6Addr->sin6_addr, &inAddrLen);
            cli->pSockAddr = reinterpret_cast<sockaddr *>(v6Addr);
        } else {
            auto *v4Addr = new sockaddr_in();
            v4Addr->sin_family = AF_INET;
            v4Addr->sin_port = BIO_ADDR_rawport(cli->pBioAddr);

            std::size_t inAddrLen = sizeof(in_addr);
            cli->sockAddrLen = sizeof(sockaddr_in);
            BIO_ADDR_rawaddress(cli->pBioAddr, &v4Addr->sin_addr, &inAddrLen);
            cli->pSockAddr = reinterpret_cast<sockaddr *>(v4Addr);
        }

        cli->addrStr = getAddrStr(cli->pSockAddr);
        STMS_INFO("New client at {} is trying to connect.", cli->addrStr);

        cli->sock = socket(BIO_ADDR_family(cli->pBioAddr), serv->pAddr->ai_socktype, serv->pAddr->ai_protocol);
        if (cli->sock == -1) {
            STMS_INFO("Failed to bind socket for new client: {}. Refusing to connect.", strerror(errno));
            return;
        }

        if (fcntl(cli->sock, F_SETFL, O_NONBLOCK) == -1) {
            STMS_INFO("Failed to set socket to non-blocking: {}. Refusing to connect.", strerror(errno));
            return;
        }

        if (BIO_ADDR_family(cli->pBioAddr) == AF_INET6) {
            if (setsockopt(cli->sock, IPPROTO_IPV6, IPV6_V6ONLY, &on, sizeof(int)) == -1) {
                STMS_INFO("Failed to setsockopt to allow IPv4 connections on client socket: {}", strerror(errno));
            }
        }

        if (setsockopt(cli->sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(int)) == -1) {
            STMS_INFO("Failed to setscokopt to reuse address on client socket "
                      "(this may result in 'Socket in Use' errors): {}", strerror(errno));
        }

        if (setsockopt(cli->sock, SOL_SOCKET, SO_REUSEPORT, &on, sizeof(int)) == -1) {
            STMS_INFO("Failed to setsockopt to reuse port on client socket "
                      "(this may result in 'Socket in Use' errors): {}", strerror(errno));
        }

        if (bind(cli->sock, serv->pAddr->ai_addr, serv->pAddr->ai_addrlen) == -1) {
            STMS_INFO("Failed to bind client socket: {}. Refusing to connect!", strerror(errno));
            return;
        }

        if (connect(cli->sock, cli->pSockAddr, cli->sockAddrLen) == -1) {
            STMS_INFO("Failed to connect client socket: {}. Refusing to connect!", strerror(errno));
        }

        BIO_set_fd(SSL_get_rbio(cli->pSsl), cli->sock, BIO_NOCLOSE);
        BIO_ctrl(SSL_get_rbio(cli->pSsl), BIO_CTRL_DGRAM_SET_CONNECTED, 0, cli->pSockAddr);

        bool doHandshake = true;
        int handshakeTimeouts = 0;
        while (doHandshake && handshakeTimeouts < serv->maxTimeouts) {
            handshakeTimeouts++;
            int handshakeStatus = SSL_accept(cli->pSsl);
            if (handshakeStatus == 1) {
                doHandshake = false;
            }
            if (handshakeStatus == 0) {
                STMS_INFO("Handshake failed with non-fatal error. Retrying.");
            }
            if (handshakeStatus < 0) {
                STMS_WARN("DTLS Handshake error!");
                handshakeStatus = handleSslGetErr(cli->pSsl, handshakeStatus);
                flushSSLErrors();
                if (handshakeStatus == -5 || handshakeStatus == -1 || handshakeStatus == -999 ||
                    handshakeStatus == -6) {
                    STMS_WARN("Dropping connection to client because of SSL_accept() error!");
                    return;
                }

                if (handshakeStatus == -3) { // Want write
                    STMS_INFO("WANT_WRITE returned from SSL_accept! Blocking until write-ready...");
                    if (!stms::net::DTLSServer::blockUntilReady(cli->sock, cli->pSsl, POLLOUT)) {
                        STMS_WARN("SSL_accpet() WANT_WRITE timed out!");
                        continue;
                    }
                } else if (handshakeStatus == -2) { // Want read
                    STMS_INFO("WANT_READ returned from SSL_accept! Blocking until read-ready...");
                    if (!stms::net::DTLSServer::blockUntilReady(cli->sock, cli->pSsl, POLLIN)) {
                        STMS_INFO("SSL_accpet() WANT_READ timed out!");
                        continue;
                    }
                }
                STMS_WARN("Retrying SSL_accept!");
            }
        }

        if (handshakeTimeouts >= serv->maxTimeouts) {
            STMS_WARN("DTLS server handshake timed out completely! Dropping connection");
            return;
        }

        cli->doShutdown = true; // Handshake completed, we can shutdown!

        timeval timeout{};
        timeout.tv_usec = (serv->timeoutMs % 1000) * 1000;
        timeout.tv_sec = serv->timeoutMs / 1000;
        // TODO: Is this necessary?
        BIO_ctrl(SSL_get_rbio(cli->pSsl), BIO_CTRL_DGRAM_SET_RECV_TIMEOUT, 0, &timeout);
        BIO_ctrl(SSL_get_rbio(cli->pSsl), BIO_CTRL_DGRAM_SET_SEND_TIMEOUT, 0, &timeout);

        std::string uuid = stms::genUUID4().buildStr();

        char certName[certAndCipherLen];
        char cipherName[certAndCipherLen];
        X509_NAME_oneline(X509_get_subject_name(SSL_get_certificate(cli->pSsl)), certName, certAndCipherLen);
        SSL_CIPHER_description(SSL_get_current_cipher(cli->pSsl), cipherName, certAndCipherLen);

        const char *compression = SSL_COMP_get_name(SSL_get_current_compression(cli->pSsl));
        const char *expansion = SSL_COMP_get_name(SSL_get_current_expansion(cli->pSsl));

        STMS_INFO("Client (addr='{}', uuid='{}') connected: {}",
                  cli->addrStr, uuid, SSL_state_string_long(cli->pSsl));
        STMS_INFO("Client (addr='{}', uuid='{}') has cert of {}", cli->addrStr, uuid, certName);
        STMS_INFO("Client (addr='{}', uuid='{}') is using cipher {}", cli->addrStr, uuid, cipherName);
        STMS_INFO("Client (addr='{}', uuid='{}') is using compression {} and expansion {}", cli->addrStr, uuid,
                  compression == nullptr ? "NULL" : compression,
                  expansion == nullptr ? "NULL" : expansion);
        {
            std::lock_guard<std::mutex> lg(serv->clientsMtx);
            cli->timeoutTimer.start();
            serv->clients[uuid] = cli;
        }

        serv->connectCallback(uuid, cli->pSockAddr);
    }

    DTLSServer::DTLSServer(stms::ThreadPool *pool, const std::string &addr, const std::string &port, bool preferV6,
                           const std::string &certPem, const std::string &keyPem, const std::string &caCert,
                           const std::string &caPath, const std::string &password) : _stms_SSLBase(true, pool, addr, port,
                                                                                                   preferV6,
                                                                                                   certPem, keyPem, caCert,
                                                                                                   caPath, password) {
        SSL_CTX_set_cookie_generate_cb(pCtx, genCookie);
        SSL_CTX_set_cookie_verify_cb(pCtx, verifyCookie);
    }

    DTLSServer::~DTLSServer() {
        // We cannot throw from a destructor (bc that is a terrible idea) so we settle for this instead.
        if (running) {
            STMS_PUSH_ERROR("DTLSServer destroyed whilst it was still running! Stopping it now...");
            stop();
        }
    }

//    DTLSServer::DTLSServer(DTLSServer &&rhs) noexcept {
//        *this = std::move(rhs);
//    }

    void DTLSServer::onStop() {
        std::lock_guard<std::mutex> lg(clientsMtx);
        for (auto &pair : clients) {
            // Lambda captures validated
            pPool->submitTask([&, capUuid = std::string(pair.first),
                                      capStr = std::string(pair.second->addrStr), this](void *in) -> void * {
                disconnectCallback(capUuid, capStr);
                return nullptr;
            }, nullptr, threadPoolPriority);
        }
        clients.clear();
    }

    bool DTLSServer::tick() {
        if (!running) {
            STMS_PUSH_WARNING("DTLSServer::tick() called when stopped! Ignoring invocation!");
            return false;
        }

        std::shared_ptr<DTLSClientRepresentation> cli = std::make_shared<DTLSClientRepresentation>();
        cli->doShutdown = false;
        cli->pBio = BIO_new_dgram(sock, BIO_NOCLOSE);

        timeval timeout{};
        timeout.tv_usec = (timeoutMs % 1000) * 1000;
        timeout.tv_sec = timeoutMs / 1000;
        BIO_ctrl(cli->pBio, BIO_CTRL_DGRAM_SET_RECV_TIMEOUT, 0, &timeout);
        BIO_ctrl(cli->pBio, BIO_CTRL_DGRAM_SET_SEND_TIMEOUT, 0, &timeout);


        cli->pSsl = SSL_new(pCtx);
        cli->pBioAddr = BIO_ADDR_new();
        SSL_set_bio(cli->pSsl, cli->pBio, cli->pBio);

        BIO_ctrl(cli->pBio, BIO_CTRL_DGRAM_MTU_DISCOVER, 0, nullptr);

        SSL_set_options(cli->pSsl, SSL_OP_COOKIE_EXCHANGE);
        SSL_clear_options(cli->pSsl, SSL_OP_NO_COMPRESSION);

        int listenStatus = DTLSv1_listen(cli->pSsl, cli->pBioAddr);

        // If it returns 0 it means no clients have tried to connect.
        if (listenStatus < 0) {
            STMS_PUSH_ERROR("Fatal error from DTLSv1_listen!");
            flushSSLErrors();
        } else if (listenStatus >= 1) {
            if (BIO_ADDR_family(cli->pBioAddr) != AF_INET6 && BIO_ADDR_family(cli->pBioAddr) != AF_INET) {
                STMS_PUSH_WARNING("A client tried to connect with an unsupported family {}! Refusing to connect!", BIO_ADDR_family(cli->pBioAddr));
            } else {
                // Lambda captures validated
                pPool->submitTask([&, capCli{cli}](void *in) -> void * {
                    handleClientConnection(capCli, this);
                    return nullptr;
                }, this, threadPoolPriority);
            }
        }

        // We must use this as modifying `clients` while we are looping through is a TERRIBLE idea

        std::lock_guard<std::mutex> lg(clientsMtx);
        for (auto &client : clients) {
            if ((SSL_get_shutdown(client.second->pSsl) & SSL_RECEIVED_SHUTDOWN) ||
                client.second->timeouts >= maxConnectionTimeouts) {
                deadClients.push(client.first);
            } else {
                if (client.second->timeoutTimer.getTime() >= timeoutMs) {
                    client.second->timeouts++;
                    client.second->timeoutTimer.reset();
                    DTLSv1_handle_timeout(client.second->pSsl);
                    STMS_INFO("Client {} timed out! (timeout #{})", client.first, client.second->timeouts);
                }

                if (recvfrom(client.second->sock, nullptr, 0, MSG_PEEK, client.second->pSockAddr,
                             &client.second->sockAddrLen) == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                    continue;  // No data could be read
                }

                if (!client.second->isReading) {
                    client.second->isReading = true;
                    // lambda captures validated
                    pPool->submitTask([&, lambCli = std::shared_ptr<DTLSClientRepresentation>(client.second),
                                              lambUUid = std::string(client.first)](void *in) -> void * {

                        bool retryRead = true;
                        int readTimeouts = 0;
                        while (retryRead && lambCli->timeouts < 1 && readTimeouts < maxTimeouts) {
                            readTimeouts++;

                            if (!stms::net::DTLSServer::blockUntilReady(lambCli->sock, lambCli->pSsl, POLLIN)) {
                                STMS_WARN("SSL_read() timed out!");
                                continue;
                            }

                            uint8_t recvBuf[maxRecvLen];
                            int readLen = SSL_read(lambCli->pSsl, recvBuf, maxRecvLen);
                            readLen = handleSslGetErr(lambCli->pSsl, readLen);

                            if (readLen > 0) {
                                lambCli->timeoutTimer.reset();
                                recvCallback(lambUUid, lambCli->pSockAddr, recvBuf, readLen);
                                retryRead = false;
                            } else if (readLen == -2) {
                                STMS_INFO("Retrying SSL_read(): WANT_READ");
                            } else if (readLen == -1 || readLen == -5 || readLen == -6) {
                                STMS_WARN("Connection to client {} closed forcefully!", lambUUid);
                                lambCli->doShutdown = false;

                                std::lock_guard<std::mutex> lgSub(clientsMtx);
                                deadClients.push(lambUUid);
                                retryRead = false;
                            } else if (readLen == -999) {
                                STMS_WARN("Client {} kicked for: Unknown error", lambUUid);

                                std::lock_guard<std::mutex> lgSub(clientsMtx);
                                deadClients.push(lambUUid);
                                retryRead = false;
                            } else {
                                STMS_WARN("Client {} SSL_read failed for the reason above! Retrying!", lambUUid);
                                // Retry.
                            }
                        }

                        if (readTimeouts >= maxTimeouts) {
                            STMS_WARN("SSL_read() timed out completely! Dropping connection!");
                            std::lock_guard<std::mutex> lgSub(clientsMtx);
                            deadClients.push(lambUUid);
                        }
                        lambCli->isReading = false;
                        return nullptr;
                    }, nullptr, threadPoolPriority);
                }
            }
        }

        while (!deadClients.empty()) {
            std::string cliUuid = deadClients.front();
            deadClients.pop();

            if (clients.find(cliUuid) == clients.end()) {
                STMS_INFO("Dead clients contained a duplicate! Ignoring duplicate...");
                return running;
            }

            // lambda captures validated
            pPool->submitTask([&, capUuid = std::string(cliUuid),
                                      capStr = std::string(clients[cliUuid]->addrStr), this](void *in) -> void * {
                disconnectCallback(capUuid, capStr);
                return nullptr;
            }, nullptr, threadPoolPriority);

            STMS_INFO("Client {} at {} disconnected!", cliUuid, clients[cliUuid]->addrStr);
            clients.erase(cliUuid);
        }

        return running;
    }

    std::future<int> DTLSServer::send(const std::string &clientUuid, const uint8_t *const msg, int msgLen) {
        std::shared_ptr<std::promise<int>> prom = std::make_shared<std::promise<int>>();
        if (!running) {
            STMS_PUSH_ERROR("DTLSServer::send() called when stopped! Dropping {} bytes!", msgLen);
            prom->set_value(-114);
            return prom->get_future();
        }

        // lambda captures validated
        pPool->submitTask([&, capProm{prom}, capUuid{clientUuid}, capMsg{msg}, capLen{msgLen}](void *) -> void * {

            clientsMtx.lock();

            if (clients.find(capUuid) == clients.end()) {
                clientsMtx.unlock();
                STMS_PUSH_ERROR("DTLSServer::send() called with invalid client uuid '{}'. Dropping {} bytes!", capUuid, msgLen);
                prom->set_value(0);
                return nullptr;
            }

            std::shared_ptr<DTLSClientRepresentation> cli = clients[capUuid];
            clientsMtx.unlock();

            int ret = -3;

            int sendTimeouts = 0;
            while (ret == -3 && sendTimeouts < maxTimeouts) {
                sendTimeouts++;
                if (!stms::net::DTLSServer::blockUntilReady(cli->sock, cli->pSsl, POLLOUT)) {
                    STMS_WARN("SSL_write() timed out!");
                    continue;
                }

                ret = SSL_write(cli->pSsl, capMsg, capLen);
                ret = handleSslGetErr(cli->pSsl, ret);

                if (ret > 0) {
                    capProm->set_value(ret);
                    return nullptr;
                }

                if (ret == -3) {
                    STMS_WARN("send() failed with WANT_WRITE! Retrying!");
                } else if (ret == -1 || ret == -5 || ret == -6) {
                    STMS_WARN("Connection to client {} at {} closed forcefully!", capUuid, cli->addrStr);
                    cli->doShutdown = false;

                    std::lock_guard<std::mutex> lg(clientsMtx);
                    deadClients.push(capUuid);
                    capProm->set_value(ret);
                    return nullptr;
                } else if (ret == -999) {
                    STMS_WARN("Kicking client {} at {} for: Unknown error", capUuid, cli->addrStr);

                    std::lock_guard<std::mutex> lg(clientsMtx);
                    deadClients.push(capUuid);
                    capProm->set_value(ret);
                    return nullptr;
                } else if (ret < 1) {
                    STMS_WARN("SSL_write failed for the reason above! Retrying!");
                }
                STMS_INFO("Retrying SSL_write!");
            }

            if (sendTimeouts >= maxTimeouts) {
                STMS_WARN("SSL_write() timed out completely! Dropping connection!");

                std::lock_guard<std::mutex> lg(clientsMtx);
                deadClients.push(capUuid);
            }
            capProm->set_value(ret);
            return nullptr;
        }, nullptr, threadPoolPriority);

        return prom->get_future();
    }

    size_t DTLSServer::getMtu(const std::string &cli) {
        std::lock_guard<std::mutex> lg(clientsMtx);
        if (clients.find(cli) == clients.end()) {
            STMS_PUSH_ERROR("DTLSServer::getMtu called with invalid client uuid '{}'!", cli);
            return 0;
        }

        return DTLS_get_data_mtu(clients[cli]->pSsl);
    }

    DTLSClientRepresentation::~DTLSClientRepresentation() {
        shutdownClient();
    }

    void DTLSClientRepresentation::shutdownClient() const {
        if (doShutdown && pSsl != nullptr) {
            int shutdownRet = 0;
            int numTries = 0;
            while (shutdownRet == 0 && numTries < sslShutdownMaxRetries) {
                numTries++;
                shutdownRet = SSL_shutdown(pSsl);
                if (shutdownRet < 0) {
                    shutdownRet = handleSslGetErr(pSsl, shutdownRet);
                    flushSSLErrors();
                    STMS_WARN("Error in SSL_shutdown (see above)!");

                    if (shutdownRet == -2) {
                        if (!stms::net::DTLSServer::blockUntilReady(sock, pSsl, POLLIN)) {
                            STMS_WARN("Reading timed out for SSL_shutdown!");
                        }
                    } else {
                        STMS_WARN("Skipping SSL_shutdown()!");
                        break;
                    }
                } else if (shutdownRet == 0) {
                    flushSSLErrors();
                    STMS_INFO("SSL_shutdown needs to be recalled! Retrying...");
                }
            }

            if (numTries >= sslShutdownMaxRetries) {
                STMS_WARN("Skipping SSL_shutdown: Timed out!");
            }
        }
        SSL_free(pSsl);
        // No need to free BIO since it is bound to the SSL object and freed when the SSL object is freed.
        BIO_ADDR_free(pBioAddr);

        if (sock != 0) {
            if (shutdown(sock, 0) == -1) {
                STMS_INFO("Failed to shutdown client representation socket: {}", strerror(errno));
            }
            if (close(sock) == -1) {
                STMS_INFO("Failed to close client representation socket: {}", strerror(errno));
            }
        }

        if (pSockAddr != nullptr) {
            if (pSockAddr->sa_family == AF_INET) {
                delete reinterpret_cast<sockaddr_in *>(pSockAddr);
            } else {
                delete reinterpret_cast<sockaddr_in6 *>(pSockAddr);
            }
        }
    }

    DTLSClientRepresentation &DTLSClientRepresentation::operator=(DTLSClientRepresentation &&rhs) noexcept {
        if (&rhs == this) {
            return *this;
        }

        shutdownClient();

        addrStr = rhs.addrStr;
        pBioAddr = rhs.pBioAddr;
        pSockAddr = rhs.pSockAddr;
        pBio = rhs.pBio;
        pSsl = rhs.pSsl;
        sock = rhs.sock;
        timeouts = rhs.timeouts;
        doShutdown = rhs.doShutdown;
        sockAddrLen = rhs.sockAddrLen;
        isReading = rhs.isReading;
        timeoutTimer = rhs.timeoutTimer;

        rhs.sock = 0;
        rhs.pSsl = nullptr;
        rhs.pBio = nullptr;
        rhs.pSockAddr = nullptr;
        rhs.pBioAddr = nullptr;

        return *this;
    }

    DTLSClientRepresentation::DTLSClientRepresentation(DTLSClientRepresentation &&rhs) noexcept {
        *this = std::move(rhs);
    }
}
