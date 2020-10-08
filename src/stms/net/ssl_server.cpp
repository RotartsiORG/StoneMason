//
// Created by grant on 4/22/20.
//

#include "stms/net/ssl_server.hpp"
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

namespace stms {

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

    static void doHandshake(const std::shared_ptr<ClientRepresentation> &cli, SSLServer *serv) {
        int handshakeTimeouts = 0;
        while (handshakeTimeouts < serv->maxTimeouts) {
            handshakeTimeouts++;
            try {
                int ret = handleSslGetErr(cli->pSsl, SSL_accept(cli->pSsl));

                if (ret == 1) {
                    handshakeTimeouts = 0;
                    STMS_INFO("Handshake completed successfully with cli at {}", cli->addrStr);
                    break;
                }
            } catch (SSLWantReadException &) {
                STMS_INFO("WANT_READ returned from SSL_accept! Blocking until read-ready and retrying.");
                serv->blockUntilReady(cli->sock, cli->pSsl, POLLIN);
            } catch (SSLWantWriteException &) {
                STMS_INFO("WANT_WRITE returned from SSL_accept! Blocking until write-ready and retrying.");
                serv->blockUntilReady(cli->sock, cli->pSsl, POLLOUT);
            } catch (SSLFatalException &) {
                STMS_WARN("Dropping connection to client because of fatal SSL_accept() error!");
                return;
            } catch (SSLException &) {
                STMS_WARN("Non-fatal Handshake error! Retrying!");
            }
        }

        if (handshakeTimeouts >= serv->maxTimeouts) {
            STMS_WARN("DTLS server handshake timed out completely! Dropping connection");
            return;
        }

        cli->doShutdown = true; // Handshake completed, we can shutdown!

        if (serv->isUdp) {
            timeval timeout{};
            timeout.tv_usec = (serv->timeoutMs % 1000) * 1000;
            timeout.tv_sec = serv->timeoutMs / 1000;
            // TODO: Is this necessary?
            BIO_ctrl(SSL_get_rbio(cli->pSsl), BIO_CTRL_DGRAM_SET_RECV_TIMEOUT, 0, &timeout);
            BIO_ctrl(SSL_get_rbio(cli->pSsl), BIO_CTRL_DGRAM_SET_SEND_TIMEOUT, 0, &timeout);
        }

        UUID uuid{eUuid4};

        char certName[certAndCipherLen];
        char cipherName[certAndCipherLen];

        auto peerCert = SSL_get_certificate(cli->pSsl);
        X509_NAME_oneline(X509_get_subject_name(peerCert), certName, certAndCipherLen);
        X509_free(peerCert); // beware dynamic allocation! why isn't this documented?!

        SSL_CIPHER_description(SSL_get_current_cipher(cli->pSsl), cipherName, certAndCipherLen);

        const char *compression = SSL_COMP_get_name(SSL_get_current_compression(cli->pSsl));
        const char *expansion = SSL_COMP_get_name(SSL_get_current_expansion(cli->pSsl));

        std::string uuidStr = uuid.buildStr();
        STMS_INFO("Client (addr='{}', uuid='{}') connected: {}",
                  cli->addrStr, uuidStr, SSL_state_string_long(cli->pSsl));
        STMS_INFO("Client (addr='{}', uuid='{}') has cert of {}", cli->addrStr, uuidStr, certName);
        STMS_INFO("Client (addr='{}', uuid='{}') is using cipher {}", cli->addrStr, uuidStr, cipherName);
        STMS_INFO("Client (addr='{}', uuid='{}') is using compression {} and expansion {}", cli->addrStr, uuidStr,
                  compression == nullptr ? "NULL" : compression,
                  expansion == nullptr ? "NULL" : expansion);
        {
            cli->timeoutTimer.start();
            std::lock_guard<std::mutex> lg(serv->clientsMtx);
            serv->clients[uuid] = cli;
        }

        serv->connectCallback(uuid, cli->pSockAddr);
    }

    static void handleDtlsConnection(const std::shared_ptr<ClientRepresentation> &cli, SSLServer *serv) {
        int on = 1;

        if (BIO_ADDR_family(cli->dtls->pBioAddr) == AF_INET6) {
            auto *v6Addr = new sockaddr_in6();
            v6Addr->sin6_family = AF_INET6;
            v6Addr->sin6_port = BIO_ADDR_rawport(cli->dtls->pBioAddr);

            std::size_t inAddrLen = sizeof(in6_addr);
            cli->sockAddrLen = sizeof(sockaddr_in6);
            BIO_ADDR_rawaddress(cli->dtls->pBioAddr, &v6Addr->sin6_addr, &inAddrLen);
            cli->pSockAddr = reinterpret_cast<sockaddr *>(v6Addr);
        } else {
            auto *v4Addr = new sockaddr_in();
            v4Addr->sin_family = AF_INET;
            v4Addr->sin_port = BIO_ADDR_rawport(cli->dtls->pBioAddr);

            std::size_t inAddrLen = sizeof(in_addr);
            cli->sockAddrLen = sizeof(sockaddr_in);
            BIO_ADDR_rawaddress(cli->dtls->pBioAddr, &v4Addr->sin_addr, &inAddrLen);
            cli->pSockAddr = reinterpret_cast<sockaddr *>(v4Addr);
        }

        cli->addrStr = getAddrStr(cli->pSockAddr);
        STMS_INFO("New client at {} is trying to connect.", cli->addrStr);

        cli->sock = socket(BIO_ADDR_family(cli->dtls->pBioAddr), serv->pAddr->ai_socktype, serv->pAddr->ai_protocol);
        if (cli->sock == -1) {
            STMS_INFO("Failed to bind socket for new client: {}. Refusing to connect.", strerror(errno));
            return;
        }

        if (fcntl(cli->sock, F_SETFL, O_NONBLOCK) == -1) {
            STMS_INFO("Failed to set socket to non-blocking: {}. Refusing to connect.", strerror(errno));
            return;
        }

        if (BIO_ADDR_family(cli->dtls->pBioAddr) == AF_INET6) {
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

        doHandshake(cli, serv);
    }

    SSLServer::SSLServer(stms::ThreadPool *pool, bool udp) : _stms_SSLBase(true, pool, udp) {
        SSL_CTX_set_cookie_generate_cb(pCtx, genCookie);
        SSL_CTX_set_cookie_verify_cb(pCtx, verifyCookie);
    }

    SSLServer::~SSLServer() {
        // We cannot throw from a destructor (bc that is a terrible idea) so we settle for this instead.
        if (running) {
            STMS_ERROR("SSLServer destroyed whilst it was still running! Stopping it now...");
            stop();
        }
    }

//    SSLServer::SSLServer(SSLServer &&rhs) noexcept {
//        *this = std::move(rhs);
//    }

    void SSLServer::onStop() {
        std::lock_guard<std::mutex> lg(clientsMtx);
        for (auto &pair : clients) {
            auto *addrCpy = new sockaddr_storage{};
            // STMS_FATAL("p = {}", stms::getAddrStr(pair.second->pSockAddr));

            std::copy(reinterpret_cast<uint8_t *>(pair.second->pSockAddr),
                      reinterpret_cast<uint8_t *>(pair.second->pSockAddr) + pair.second->sockAddrLen,
                      reinterpret_cast<uint8_t *>(addrCpy));
            // STMS_FATAL("p2 = {}", stms::getAddrStr(reinterpret_cast<const sockaddr *>(addrCpy)));

            pPool->submitTask([&, capUuid = UUID{pair.first},
                                      capAddr{addrCpy}, this]() {
                // STMS_FATAL("p3 = {}", stms::getAddrStr(reinterpret_cast<const sockaddr *>(capAddr)));
                disconnectCallback(capUuid, reinterpret_cast<sockaddr *>(capAddr));
                delete capAddr;
            });
        }
        clients.clear();
    }

    bool SSLServer::tick() {
        if (!running) {
            STMS_WARN("SSLServer::tick() called when stopped! Ignoring invocation!");
            return false;
        }

        std::shared_ptr<ClientRepresentation> cli = std::make_shared<ClientRepresentation>();
        cli->serv = this;

        if (isUdp) {
            cli->dtls = new ClientRepresentation::DTLSSpecific{};
            cli->dtls->pBio = BIO_new_dgram(sock, BIO_NOCLOSE);

            timeval timeout{};
            timeout.tv_usec = (timeoutMs % 1000) * 1000;
            timeout.tv_sec = timeoutMs / 1000;
            BIO_ctrl(cli->dtls->pBio, BIO_CTRL_DGRAM_SET_RECV_TIMEOUT, 0, &timeout);
            BIO_ctrl(cli->dtls->pBio, BIO_CTRL_DGRAM_SET_SEND_TIMEOUT, 0, &timeout);


            cli->pSsl = SSL_new(pCtx);
            cli->dtls->pBioAddr = BIO_ADDR_new();
            SSL_set_bio(cli->pSsl, cli->dtls->pBio, cli->dtls->pBio);

            BIO_ctrl(cli->dtls->pBio, BIO_CTRL_DGRAM_MTU_DISCOVER, 0, nullptr);

            SSL_set_options(cli->pSsl, SSL_OP_COOKIE_EXCHANGE);
            SSL_clear_options(cli->pSsl, SSL_OP_NO_COMPRESSION);

            int listenStatus = DTLSv1_listen(cli->pSsl, cli->dtls->pBioAddr);

            // If it returns 0 it means no clients have tried to connect.
            if (listenStatus < 0) {
                STMS_ERROR("Fatal error from DTLSv1_listen!");
                flushSSLErrors();
            } else if (listenStatus >= 1) {
                if (BIO_ADDR_family(cli->dtls->pBioAddr) != AF_INET6 &&
                    BIO_ADDR_family(cli->dtls->pBioAddr) != AF_INET) {
                    STMS_WARN("A client tried to connect with an unsupported family {}! Refusing to connect!",
                                      BIO_ADDR_family(cli->dtls->pBioAddr));
                } else {
                    // Lambda captures validated
                    pPool->submitTask([&, capCli{cli}]() {
                        handleDtlsConnection(capCli, this);
                    });
                }
            }
        } else {

            auto storage = sockaddr_storage{};
            cli->sockAddrLen = sizeof(sockaddr_storage);

            // We use accept() instead of SSL_stateless as we are using TCP and source IPs are already validated
            // in the tcp handshake. https://www.openssl.org/docs/man1.1.1/man3/SSL_stateless.html
            cli->sock = accept(sock, reinterpret_cast<sockaddr *>(&storage), &cli->sockAddrLen);
            // accept() sets the size of `sockAddrLen`

            if (cli->sock < 1) {
                // if errno is one of these, then there's simply no client
                if (!(errno == EAGAIN || errno == EWOULDBLOCK)) {
                    STMS_WARN("accept() failed: {}", strerror(errno));
                }

                goto skipConnect; // Forgive me (looks around for velociraptors)
            }

            // Prayin' that this works.
            if (storage.ss_family == AF_INET) {
                auto in4Addr = new sockaddr_in();
                *in4Addr = *reinterpret_cast<sockaddr_in *>(&storage);
                cli->pSockAddr = reinterpret_cast<sockaddr *>(in4Addr);

                if (cli->pSockAddr->sa_family != AF_INET) {
                    throw std::runtime_error("family mismatch");
                }

            } else if (storage.ss_family == AF_INET6) {
                auto in6Addr = new sockaddr_in6();
                *in6Addr = *reinterpret_cast<sockaddr_in6 *>(&storage);
                cli->pSockAddr = reinterpret_cast<sockaddr *>(in6Addr);
                if (cli->pSockAddr->sa_family != AF_INET6) {
                    throw std::runtime_error("family mismatch");
                }
            } else {
                STMS_WARN("Client tried to connect with bad proto {}! Dropping connection!", storage.ss_family);
                goto skipConnect;
            }

            cli->addrStr = getAddrStr(cli->pSockAddr);
            STMS_INFO("New TCP client at {} is trying to connect.", cli->addrStr);

            cli->pSsl = SSL_new(pCtx);
            SSL_set_fd(cli->pSsl, cli->sock);

            pPool->submitTask([&, capCli{cli}]() {
               doHandshake(capCli, this);
            });
        }

        skipConnect:

        // We must use this as modifying `clients` while we are looping through is a TERRIBLE idea

        std::lock_guard<std::mutex> lg(clientsMtx);
        for (auto &client : clients) {

            if (SSL_get_shutdown(client.second->pSsl) & SSL_RECEIVED_SHUTDOWN) {
                deadClients.push(client.first);
            } else {
                if (client.second->timeoutTimer.getTime() >= static_cast<float>(timeoutMs)) {
                    if (isUdp) { DTLSv1_handle_timeout(client.second->pSsl); }
                    STMS_INFO("Client {} timed out! Dropping connection!", client.first.buildStr());
                    deadClients.push(client.first);
                    continue;
                }

                // recvfrom can be used with both TCP & UDP (i hope i haven't been lied to by the man pages)
                if (recvfrom(client.second->sock, nullptr, 0, MSG_PEEK, nullptr, nullptr) == -1
                    && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                    continue;  // No data could be read
                }

                if (!client.second->isReading) {
                    client.second->isReading = true;
                    // lambda captures validated
                    pPool->submitTask([&, lambCli = std::shared_ptr<ClientRepresentation>(client.second),
                                              lambUUid = UUID{client.first}]() {

                        int readTimeouts = 0;
                        while (readTimeouts < maxTimeouts) {
                            readTimeouts++;

                            try {
                                uint8_t recvBuf[maxRecvLen];
                                int readLen = handleSslGetErr(lambCli->pSsl, SSL_read(lambCli->pSsl, recvBuf, maxRecvLen));

                                if (readLen > 0) {
                                    readTimeouts = 0;
                                    lambCli->timeoutTimer.reset();
                                    recvCallback(lambUUid, lambCli->pSockAddr, recvBuf, readLen);
                                    break;
                                }
                            } catch (SSLWantWriteException &) {
                                STMS_INFO("SSL_read() returned WANT_WRITE. Blocking then retrying!");
                                blockUntilReady(lambCli->sock, lambCli->pSsl, POLLOUT);
                            } catch (SSLWantReadException &) {
                                STMS_INFO("SSL_read() returned WANT_READ. Blocking then retrying!");
                                blockUntilReady(lambCli->sock, lambCli->pSsl, POLLIN);
                            } catch (SSLFatalException &) {
                                STMS_WARN("Fatal SSL_read() exception occurred! Disconnecting client {}", lambUUid.buildStr());
                                lambCli->doShutdown = false;

                                std::lock_guard<std::mutex> lgSub(clientsMtx);
                                deadClients.push(lambUUid);
                                break;
                            } catch (SSLException &) {
                                STMS_WARN("Client {} SSL_read failed for the reason above! Retrying!", lambUUid.buildStr());
                            }
                        }

                        if (readTimeouts >= maxTimeouts) {
                            STMS_WARN("SSL_read() timed out completely! Dropping connection!");
                            std::lock_guard<std::mutex> lgSub(clientsMtx);
                            deadClients.push(lambUUid);
                        }
                        lambCli->isReading = false;
                    });
                }
            }
        }

        // We use the same mutex to protect deadClients & clients. Transition to 2 separate mutexes.
        while (!deadClients.empty()) {
            UUID cliUuid = deadClients.front();
            deadClients.pop();

            if (clients.find(cliUuid) == clients.end()) {
                STMS_INFO("Dead clients contained a non-existent client! Ignoring...");
                continue;
            }

            std::shared_ptr<ClientRepresentation> cliObj = clients[cliUuid];

            auto *addrCpy = new sockaddr_storage{};
            std::copy(reinterpret_cast<uint8_t *>(cliObj->pSockAddr),
                      reinterpret_cast<uint8_t *>(cliObj->pSockAddr) + cliObj->sockAddrLen,
                      reinterpret_cast<uint8_t *>(addrCpy));

            // lambda captures validated
            pPool->submitTask([&, capUuid = UUID(cliUuid),
                                      capAddr{addrCpy}, this]() {
                disconnectCallback(capUuid, reinterpret_cast<sockaddr *>(capAddr));
                delete addrCpy;
            });

            STMS_INFO("Client {} at {} disconnected!", cliUuid.buildStr(), cliObj->addrStr);
            clients.erase(cliUuid);
        }

        return running;
    }

    std::future<int> SSLServer::send(const UUID &clientUuid, const uint8_t *const msg, int msgLen, bool cpy) {
        std::shared_ptr<std::promise<int>> prom = std::make_shared<std::promise<int>>();
        if (!running) {
            STMS_ERROR("SSLServer::send() called when stopped! Dropping {} bytes!", msgLen);
            prom->set_value(-1);
            return prom->get_future();
        }

        uint8_t *passIn{};
        if (cpy) {
            passIn = new uint8_t[msgLen];
            std::copy(msg, msg + msgLen, passIn);
        } else {
            // I am forced to do this as std::copy would be impossible otherwise. We don't actually do anything with
            // the non-const-ness though.
            passIn = const_cast<uint8_t *>(msg);
        }
        // lambda captures validated
        pPool->submitTask([&, capProm{prom}, capUuid{clientUuid}, capMsg{passIn}, capLen{msgLen}, capCpy{cpy}]() {

            std::unique_lock<std::mutex> clg(clientsMtx);

            if (clients.find(capUuid) == clients.end()) {
                clg.unlock();
                STMS_ERROR("SSLServer::send() called with invalid client uuid '{}'. Dropping {} bytes!", capUuid.buildStr(), msgLen);
                prom->set_value(0);
                return;
            }

            std::shared_ptr<ClientRepresentation> cli = clients[capUuid];
            clg.unlock();

            int sendTimeouts = 0;
            while (sendTimeouts < maxTimeouts) {
                sendTimeouts++;

                try {
                    int ret = handleSslGetErr(cli->pSsl, SSL_write(cli->pSsl, capMsg, capLen));

                    if (ret > 0) {
                        sendTimeouts = 0;
                        capProm->set_value(ret);
                        cli->timeoutTimer.reset();
                        break;
                    }
                } catch (SSLWantReadException &) {
                    STMS_WARN("send() failed with WANT_READ! Retrying!");
                    blockUntilReady(cli->sock, cli->pSsl, POLLIN);
                } catch (SSLWantWriteException &) {
                    STMS_WARN("send() failed with WANT_WRITE! Retrying!");
                    blockUntilReady(cli->sock, cli->pSsl, POLLOUT);
                } catch (SSLFatalException &) {
                    STMS_WARN("Connection to client {} at {} closed forcefully! (Fatal SSL_read() error!)", capUuid.buildStr(), cli->addrStr);
                    cli->doShutdown = false;

                    std::lock_guard<std::mutex> lg(clientsMtx);
                    deadClients.push(capUuid);
                    capProm->set_value(-2);
                    break;
                } catch (SSLException &) {
                    STMS_WARN("SSL_write failed for the reason above! Retrying!");
                }
            }

            if (capCpy) {
                delete[] capMsg;
            }

            if (sendTimeouts >= maxTimeouts) {
                STMS_WARN("SSL_write() timed out completely! Dropping connection!");

                std::lock_guard<std::mutex> lg(clientsMtx);
                deadClients.push(capUuid);
                capProm->set_value(-3);
            }
        });

        return prom->get_future();
    }

    size_t SSLServer::getMtu(const UUID &cli) {
        if (!isUdp) {
            STMS_WARN("SSLServer::getMtu() called when the server is TLS not DTLS! Ignoring invocation...");
            return 0;
        }

        std::lock_guard<std::mutex> lg(clientsMtx);
        if (clients.find(cli) == clients.end()) {
            STMS_ERROR("SSLServer::getMtu called with invalid client uuid '{}'!", cli.buildStr());
            return 0;
        }

        return DTLS_get_data_mtu(clients[cli]->pSsl);
    }

    void SSLServer::waitEvents(int toMs) {
        // sleep for a bit and wait for clients that connected in the previous tick to become noticed.
        std::this_thread::sleep_for(std::chrono::milliseconds(waitEventsSleepAmount));

        std::vector<pollfd> toPoll;
        toPoll.reserve(getNumClients() + 1);

        for (const auto &c : clients) {
            pollfd cliPollFd{};
            cliPollFd.events = POLLIN;
            cliPollFd.fd = c.second->sock;
            toPoll.push_back(cliPollFd);
        }

        pollfd servPollFd{};
        servPollFd.events = POLLIN;
        servPollFd.fd = sock;
        toPoll.emplace_back(servPollFd);

        toPoll.shrink_to_fit();
        poll(toPoll.data(), toPoll.size(), toMs);
    }

    void SSLServer::waitEventsFrom(const std::vector<UUID> &toWait, bool unblockForIncoming, int to) {
        // sleep for a bit and wait for clients that connected in the previous tick to become noticed.
        std::this_thread::sleep_for(std::chrono::milliseconds(waitEventsSleepAmount));

        std::vector<pollfd> toPoll;
        toPoll.reserve(getNumClients() + 1);

        for (const auto &c : toWait) {
            pollfd cliPollFd{};
            cliPollFd.events = POLLIN;

            std::lock_guard<std::mutex> lg(clientsMtx);
            if (clients.find(c) == clients.end()) {
                STMS_WARN("There is no client with UUID {}! Ignoring this bad UUID passed into `waitEventsFrom`...", c.buildStr());
                continue;
            }

            cliPollFd.fd = clients[c]->sock;
            toPoll.push_back(cliPollFd);
        }

        if (unblockForIncoming) {
            pollfd servPollFd{};
            servPollFd.events = POLLIN;
            servPollFd.fd = sock;
            toPoll.emplace_back(servPollFd);
        }

        toPoll.shrink_to_fit();
        poll(toPoll.data(), toPoll.size(), to);
    }

    std::vector<UUID> SSLServer::getClientUuids() {
        std::vector<UUID> ret;

        std::lock_guard<std::mutex> lg(clientsMtx);
        ret.reserve(clients.size());
        for (const auto &cliPair : clients) {
            ret.emplace_back(cliPair.first);
        }

        return ret;
    }

    UUID SSLServer::refreshUuid(const UUID &client) {
        UUID newUuid(eUuid4);

        if (!setNewUuid(client, newUuid)) {
            return UUID{}; // return empty uuid as the client doesnt exist
        }

        return newUuid;
    }

    bool SSLServer::setNewUuid(const UUID &old, const UUID &newUuid) {
        std::lock_guard<std::mutex> lg(clientsMtx);
        if (clients.find(old) == clients.end()) {
            STMS_WARN("Requested uuid edit {} -> {} failed: Client non-existent.", old.buildStr(), newUuid.buildStr());
            return false;
        }

        std::shared_ptr<ClientRepresentation> clientValue = clients[old];
        clients.erase(old); // this will not trigger the destructor as we saved a reference in `clientValue`
        clients[newUuid] = clientValue;
        return true;
    }

    void SSLServer::kickClient(const UUID &cliId) {
        std::lock_guard<std::mutex> lg(clientsMtx);
        deadClients.emplace(cliId);
    }

    ClientRepresentation::~ClientRepresentation() {
        shutdownClient();
    }

    void ClientRepresentation::shutdownClient() {
        if (doShutdown && pSsl != nullptr) {
            int numTries = 0;
            while (numTries < sslShutdownMaxRetries) {
                numTries++;

                try {
                    int shutdownRet = handleSslGetErr(pSsl,  SSL_shutdown(pSsl));
                    if (shutdownRet == 0) {
                        STMS_INFO("Waiting to hear back after SSL_shutdown... retrying");
                        continue;
                    } else if (shutdownRet == 1) {
                        break;
                    }
                } catch (SSLWantWriteException &) {
                    STMS_WARN("SSL_shutdown() returned WANT_WRITE! Blocking...");
                    serv->blockUntilReady(sock, pSsl, POLLOUT);
                } catch (SSLWantReadException &) {
                    STMS_WARN("SSL_shutdown() returned WANT_READ! Blocking...");
                    serv->blockUntilReady(sock, pSsl, POLLIN);
                } catch (SSLFatalException &) {
                    STMS_WARN("Fatal exception during SSL_shutdown()! Skipping it!");
                    break; // just give up lol
                } catch (SSLException &) {
                    STMS_INFO("Retrying SSL_shutdown()!");
                }
            }

            if (numTries >= sslShutdownMaxRetries) {
                STMS_WARN("Skipping SSL_shutdown: Timed out!");
            }

            // No need to free BIO since it is bound to the SSL object and freed when the SSL object is freed.
            SSL_free(pSsl);
            pSsl = nullptr;
        }

        if (dtls != nullptr) {
            BIO_ADDR_free(dtls->pBioAddr);
            delete dtls;
        }

        if (sock > 0) {
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

    ClientRepresentation &ClientRepresentation::operator=(ClientRepresentation &&rhs) noexcept {
        if (&rhs == this) {
            return *this;
        }

        shutdownClient();

        addrStr = rhs.addrStr;
        pSockAddr = rhs.pSockAddr;
        sockAddrLen = rhs.sockAddrLen;
        pSsl = rhs.pSsl;
        sock = rhs.sock;
        doShutdown = rhs.doShutdown;
        isReading = rhs.isReading;
        timeoutTimer = rhs.timeoutTimer;
        serv = rhs.serv;

        if (rhs.dtls != nullptr) {
            dtls = rhs.dtls;
            rhs.dtls = nullptr;
        }

        rhs.sock = 0;
        rhs.pSsl = nullptr;
        rhs.pSockAddr = nullptr;

        return *this;
    }

    ClientRepresentation::ClientRepresentation(ClientRepresentation &&rhs) noexcept {
        *this = std::move(rhs);
    }
}
