//
// Created by grant on 4/22/20.
//

#include "stms/net/dtls_server.hpp"
#include "stms/util.hpp"
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

    static constexpr unsigned certAndCipherLen = 256;
    static bool secretCookieInit = false;
    static uint8_t secretCookie[STMS_DTLS_COOKIE_SECRET_LEN];

    struct HashAddr {
        unsigned short port;
        uint8_t addr[INET6_ADDRSTRLEN];
    };

    static int getPassword(char *dest, int size, int flag, void *pass) {
        char *strPass = reinterpret_cast<char *>(pass);
        int passLen = strlen(strPass);

        strncpy(dest, strPass, size);
        dest[passLen - 1] = '\0';
        return passLen;
    }

    static int verifyCert(int ok, X509_STORE_CTX *ctx) {
        char name[certAndCipherLen];

        X509 *cert = X509_STORE_CTX_get_current_cert(ctx);
        X509_NAME_oneline(X509_get_subject_name(cert), name, certAndCipherLen);
        if (ok) {
            STMS_INFO("Trusting certificate: {}", name);
        } else {
            STMS_INFO("Rejecting certificate: {}", name);
        }
        return ok;
    }

    static bool hashSSL(SSL *ssl, unsigned *len, uint8_t result[]) {
        if (!secretCookieInit) {
            if (RAND_bytes(secretCookie, sizeof(uint8_t) * STMS_DTLS_COOKIE_SECRET_LEN) != 1) {
                STMS_WARN(
                        "OpenSSL RNG is faulty! Using C++ RNG instead! This may leave you vulnerable to DDoS attacks!");
                for (unsigned char &i : secretCookie) {
                    i = stms::intRand(0, UINT8_MAX);
                }
            }
            secretCookieInit = true;
        }

        union {
            struct sockaddr_storage ss;
            struct sockaddr_in6 s6;
            struct sockaddr_in s4;
        } peer{};

        BIO_dgram_get_peer(SSL_get_rbio(ssl), &peer);

        size_t addrLen = sizeof(unsigned short);
        HashAddr addr{};
        if (peer.ss.ss_family == AF_INET6) {
            addrLen += sizeof(sockaddr_in6);
            addr.port = peer.s6.sin6_port;
            memcpy(&addr.addr, &peer.s6.sin6_addr, sizeof(sockaddr_in6));
        } else if (peer.ss.ss_family == AF_INET) {
            addrLen += sizeof(sockaddr_in);
            addr.port = peer.s4.sin_port;
            memcpy(&addr.addr, &peer.s4.sin_addr, sizeof(sockaddr_in));
        } else {
            STMS_WARN("Cannot generate cookie for client with invalid family!");
            return false;
        }

        *len = DTLS1_COOKIE_LENGTH - 1;
        // TODO: Should i really be using SHA1?
        HMAC(EVP_sha1(), secretCookie, STMS_DTLS_COOKIE_SECRET_LEN, (uint8_t *) &addr, addrLen, result, len);
        return true;
    }

    static int genCookie(SSL *ssl, unsigned char *cookie, unsigned int *cookieLen) {
        uint8_t result[DTLS1_COOKIE_LENGTH - 1];
        if (!hashSSL(ssl, cookieLen, result)) {
            return 0;
        }
        memcpy(cookie, result, *cookieLen);

        return 1;
    }

    static int verifyCookie(SSL *ssl, const unsigned char *cookie, unsigned int cookieLen) {
        if (!secretCookieInit) {
            STMS_WARN("Cookie received before a cookie was generated! Are you under a DDoS attack?");
            return 0;
        }
        unsigned expectedLen;
        uint8_t result[DTLS1_COOKIE_LENGTH - 1];
        if (!hashSSL(ssl, &expectedLen, result)) {
            return 0;
        }

        if (cookieLen == expectedLen && memcmp(result, cookie, expectedLen) == 0) {
            return 1;
        }

        STMS_WARN("Received an invalid cookie! Are you under a DDoS attack?");
        return 0;

    }

    static void handleClientConnection(const std::shared_ptr<DTLSClientRepresentation> &cli, DTLSServer *serv) {
        int on = 1;

        if (BIO_ADDR_family(cli->pBioAddr) == AF_INET6) {
            auto *v6Addr = new sockaddr_in6{};
            v6Addr->sin6_family = AF_INET6;
            v6Addr->sin6_port = BIO_ADDR_rawport(cli->pBioAddr);

            std::size_t inAddrLen = sizeof(in6_addr);
            cli->sockAddrLen = sizeof(sockaddr_in6);
            BIO_ADDR_rawaddress(cli->pBioAddr, &v6Addr->sin6_addr, &inAddrLen);
            cli->pSockAddr = reinterpret_cast<sockaddr *>(v6Addr);
        } else {
            auto *v4Addr = new sockaddr_in{};
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
            STMS_INFO(
                    "Failed to setscokopt to reuse address on client socket (this may result in 'Socket in Use' errors): {}",
                    strerror(errno));
        }

        if (setsockopt(cli->sock, SOL_SOCKET, SO_REUSEPORT, &on, sizeof(int)) == -1) {
            STMS_INFO(
                    "Failed to setsockopt to reuse port on client socket (this may result in 'Socket in Use' errors): {}",
                    strerror(errno));
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
        while (doHandshake) {
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
                    STMS_WARN("Calling SSL_write() in response to SSL_accept()");
                    int writeRet = -3;
                    while (writeRet == -3) {
                        writeRet = SSL_write(cli->pSsl, &on, 0);
                        writeRet = handleSslGetErr(cli->pSsl, writeRet);
                        if (writeRet == -3) {
                            STMS_WARN("SSL_write() in response to SSL_accept() failed with WANT_WRITE! Retrying!");
                        }
                    }
                    if (writeRet == -1 || writeRet == -5 || writeRet == -999 || writeRet == -6) {
                        STMS_WARN("Fatal SSL_write() error in response to SSL_accept()!");
                    } else if (writeRet < 1) {
                        STMS_WARN("SSL_write failed for the reason above!");
                    }
                } else if (handshakeStatus == -2) { // Want read
                    STMS_WARN("Calling SSL_read() in response to SSL_accept()");

                    pollfd cliPollFd{};
                    cliPollFd.events = POLLIN;
                    cliPollFd.fd = cli->sock;

                    DTLSv1_get_timeout(cli->pSsl, &serv->timeout);
                    serv->timeoutMs = static_cast<int>((serv->timeout.tv_sec * 1000) + (serv->timeout.tv_usec / 1000));

                    STMS_INFO("DTLS read timeout is set for {} milliseconds", serv->timeoutMs);

                    if (poll(&cliPollFd, 1, static_cast<int>(serv->timeoutMs)) == 0) {
                        STMS_WARN("poll() timed out!");
                        DTLSv1_handle_timeout(cli->pSsl);
                    }

                    if (!(cliPollFd.revents & POLLIN)) {
                        STMS_WARN("Not ready to read yet! Retrying...");
                        continue;
                    }

                    bool retryRead = true;
                    int thisAttemptTimeouts = 0;
                    while (retryRead && thisAttemptTimeouts < STMS_DTLS_MAX_TIMEOUTS) {
                        uint8_t recvBuf[STMS_DTLS_MAX_RECV_LEN];
                        int readLen = SSL_read(cli->pSsl, recvBuf, STMS_DTLS_MAX_RECV_LEN);
                        readLen = handleSslGetErr(cli->pSsl, readLen);

                        if (readLen > 0) {
                            STMS_WARN("Discarding {} bytes from client trying to connect!", readLen);
                            retryRead = false;
                        } else if (readLen == -2) {
                            if (BIO_ctrl(SSL_get_rbio(cli->pSsl), BIO_CTRL_DGRAM_GET_RECV_TIMER_EXP, 0, nullptr)) {
                                thisAttemptTimeouts++;
                                STMS_INFO("SSL_read() timed out for client trying to connect! (timeout #{})",
                                          thisAttemptTimeouts);
                            } else {
                                retryRead = true;
                            }
                        } else if (readLen == -1 || readLen == -5 || readLen == -6) {
                            STMS_WARN("Connection to client trying to connect closed forcefully!");
                            cli->doShutdown = false;
                            return;
                        } else if (readLen == -999) {
                            STMS_WARN("Client trying to connect kicked for: Unknown error");
                            return;
                        } else {
                            STMS_WARN("Client trying to connect: SSL_read failed for the reason above!");
                            retryRead = true;
                        }
                    }
                }
            }
        }

        cli->doShutdown = true; // Handshake completed, we can shutdown!

        BIO_ctrl(SSL_get_rbio(cli->pSsl), BIO_CTRL_DGRAM_SET_RECV_TIMEOUT, 0, &serv->timeout);
        BIO_ctrl(SSL_get_rbio(cli->pSsl), BIO_CTRL_DGRAM_SET_SEND_TIMEOUT, 0, &serv->timeout);

        std::string uuid = stms::genUUID4().getStr();

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
                           const std::string &caPath, const std::string &password) : wantV6(preferV6), pPool(pool) {

        if (!password.empty()) {
            this->password = new char[password.length()];
            strcpy(this->password, password.c_str());
        } else {
            this->password = new char;
            *this->password = 0;
        }

        timeout.tv_sec = STMS_DTLS_SEC_TIMEOUT;
        timeout.tv_usec = STMS_DTLS_USEC_TIMEOUT;

        pCtx = SSL_CTX_new(DTLS_server_method());

        SSL_CTX_set_default_passwd_cb_userdata(pCtx, this->password);
        SSL_CTX_set_default_passwd_cb(pCtx, getPassword);

        flushSSLErrors();
        if (!SSL_CTX_use_certificate_chain_file(pCtx, certPem.c_str())) {
            STMS_INFO("Failed to set public cert chain for DTLS Server (cert='{}')", certPem);
            handleSSLError();
        }
        if (!SSL_CTX_use_PrivateKey_file(pCtx, keyPem.c_str(), SSL_FILETYPE_PEM)) {
            STMS_INFO("Failed to set private key for DTLS Server (key='{}')", keyPem);
            handleSSLError();
        }
        if (!SSL_CTX_check_private_key(pCtx)) {
            STMS_INFO("Public cert and private key mismatch in DTLS server"
                      "for public cert '{}' and private key '{}'", certPem, keyPem);
            handleSSLError();
        }

        if (!SSL_CTX_load_verify_locations(pCtx, caCert.empty() ? nullptr : caCert.c_str(),
                                           caPath.empty() ? nullptr : caPath.c_str())) {
            STMS_INFO("Failed to set the CA certs and paths for DTLS server! Path='{}', cert='{}'", caPath, caCert);
            handleSSLError();
        }

        SSL_CTX_set_verify(pCtx, SSL_VERIFY_PEER, verifyCert);
        SSL_CTX_set_cookie_generate_cb(pCtx, genCookie);
        SSL_CTX_set_cookie_verify_cb(pCtx, verifyCookie);

        SSL_CTX_set_mode(pCtx, SSL_MODE_AUTO_RETRY | SSL_MODE_ASYNC);
        SSL_CTX_clear_mode(pCtx, SSL_MODE_ENABLE_PARTIAL_WRITE);
        SSL_CTX_set_options(pCtx, SSL_OP_COOKIE_EXCHANGE | SSL_OP_NO_ANTI_REPLAY);
        SSL_CTX_clear_options(pCtx, SSL_OP_NO_COMPRESSION);

        addrinfo hints{};
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_DGRAM;
        hints.ai_protocol = IPPROTO_UDP;
        if (addr == "any") {
            hints.ai_flags = AI_PASSIVE;
        }

        STMS_INFO("Creating new DTLS server to be hosted on {}:{}", addr, port);
        int lookupStatus = getaddrinfo(addr == "any" ? nullptr : addr.c_str(), port.c_str(), &hints, &pAddrCandidates);
        if (lookupStatus != 0) {
            STMS_INFO("Failed to resolve ip address of {}:{} (ERRNO: {})", addr, port, gai_strerror(lookupStatus));
        }
    }

    DTLSServer::~DTLSServer() {
        delete[] password;
        stop();
        SSL_CTX_free(pCtx);
        freeaddrinfo(pAddrCandidates);
    }

//    DTLSServer::DTLSServer(DTLSServer &&rhs) noexcept {
//        *this = std::move(rhs);
//    }

    void DTLSServer::start() {
        if (!pPool->isRunning()) {
            STMS_CRITICAL(
                    "DTLSServer started with a stopped thread pool! This will result in nothing being proccessed!");
            STMS_CRITICAL(
                    "Please explicitly start it before calling DTLSServer::start() and leave it running until the server stops!");
            STMS_CRITICAL("Refusing to start DTLSServer!");
            return;
        }

        isRunning = true;

        int i = 0;
        for (addrinfo *p = pAddrCandidates; p != nullptr; p = p->ai_next) {
            if (p->ai_family == AF_INET6) {
                if (tryAddr(p, i)) {
                    if (wantV6) {
                        STMS_INFO("Using Candidate {} because it is IPv6", i);
                        return;
                    }
                }
            } else if (p->ai_family == AF_INET) {
                if (tryAddr(p, i)) {
                    if (!wantV6) {
                        STMS_INFO("Using Candidate {} because it is IPv4", i);
                        return;
                    }
                }
            } else {
                STMS_INFO("Candidate {} has unsupported family. Ignoring.", i);
            }
            i++;
        }

        STMS_WARN("No IP addresses resolved from supplied address and port can be used to host the DTLS Server!");
        isRunning = false;
    }

    void DTLSServer::stop() {
        isRunning = false;

        {
            std::lock_guard<std::mutex> lg(clientsMtx);
            clients.clear();
        }

        if (serverSock == 0) {
            return;
        }

        if (shutdown(serverSock, 0) == -1) {
            STMS_INFO("Failed to shutdown server socket: {}", strerror(errno));
        }
        if (close(serverSock) == -1) {
            STMS_INFO("Failed to close server socket: {}", strerror(errno));
        }
        serverSock = 0;
        STMS_INFO("DTLS server stopped. Resources freed.");
    }

    bool DTLSServer::tick() {

        if (!isRunning) {
            return false;
        }

        std::shared_ptr<DTLSClientRepresentation> cli = std::make_shared<DTLSClientRepresentation>();
        cli->doShutdown = false;
        cli->pBio = BIO_new_dgram(serverSock, BIO_NOCLOSE);
        BIO_ctrl(cli->pBio, BIO_CTRL_DGRAM_SET_RECV_TIMEOUT, 0, &timeout);
        BIO_ctrl(cli->pBio, BIO_CTRL_DGRAM_SET_SEND_TIMEOUT, 0, &timeout);
        cli->pSsl = SSL_new(pCtx);
        cli->pBioAddr = BIO_ADDR_new();
        SSL_set_bio(cli->pSsl, cli->pBio, cli->pBio);

        BIO_ctrl(cli->pBio, BIO_CTRL_DGRAM_MTU_DISCOVER, 0, nullptr);

        SSL_set_options(cli->pSsl, SSL_OP_COOKIE_EXCHANGE);
        SSL_clear_options(cli->pSsl, SSL_OP_NO_COMPRESSION);

        int listenStatus = DTLSv1_listen(cli->pSsl, cli->pBioAddr);

        if (listenStatus < 0) {
            STMS_WARN("Fatal error from DTLSv1_listen!");
        } else if (listenStatus >= 1) {
            if (BIO_ADDR_family(cli->pBioAddr) != AF_INET6 && BIO_ADDR_family(cli->pBioAddr) != AF_INET) {
                STMS_INFO("A client tried to connect with an unsupported family! Refusing to connect.");
            } else {
                pPool->submitTask([=, capCli{cli}, this](void *in) -> void * {
                    handleClientConnection(capCli, this);
                    return nullptr;
                }, this, 8);
            }
        }

        // We must use this as modifying `clients` while we are looping through is a TERRIBLE idea

        std::lock_guard<std::mutex> lg(clientsMtx);
        for (auto &client : clients) {
            if ((SSL_get_shutdown(client.second->pSsl) & SSL_RECEIVED_SHUTDOWN) ||
                client.second->timeouts >= STMS_DTLS_MAX_TIMEOUTS) {
                deadClients.push(client.first);
            } else {
                char peekBuf[STMS_DTLS_MAX_RECV_LEN];

                DTLSv1_get_timeout(cli->pSsl, &timeout);
                timeoutMs = static_cast<int>((timeout.tv_sec * 1000) + (timeout.tv_usec / 1000));

                if (client.second->timeoutTimer.getTime() >= timeoutMs) {
                    client.second->timeouts++;
                    client.second->timeoutTimer.reset();
                    DTLSv1_handle_timeout(client.second->pSsl);
                    STMS_INFO("Client {} timed out! (timeout #{})", client.first, client.second->timeouts);
                }

                if (recvfrom(client.second->sock, peekBuf, STMS_DTLS_MAX_RECV_LEN, MSG_PEEK, client.second->pSockAddr,
                             &client.second->sockAddrLen) == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                    continue;  // No data could be read
                }

                if (!client.second->isReading) {
                    client.second->isReading = true;
                    pPool->submitTask([=, lambCli{client.second}, lambUUid{client.first}, this](void *in) -> void * {
                        bool retryRead = true;
                        while (retryRead && lambCli->timeouts < STMS_DTLS_MAX_TIMEOUTS) {
                            uint8_t recvBuf[STMS_DTLS_MAX_RECV_LEN];
                            int readLen = SSL_read(lambCli->pSsl, recvBuf, STMS_DTLS_MAX_RECV_LEN);
                            readLen = handleSslGetErr(lambCli->pSsl, readLen);

                            if (readLen > 0) {
                                lambCli->timeoutTimer.reset();
                                recvCallback(lambUUid, lambCli->pSockAddr, recvBuf, readLen);
                                retryRead = false;
                            } else if (readLen == -2) {
                                if (BIO_ctrl(SSL_get_rbio(lambCli->pSsl), BIO_CTRL_DGRAM_GET_RECV_TIMER_EXP, 0,
                                             nullptr)) {
                                    STMS_INFO("SSL_read() timed out for client {} (Ignoring this)", lambUUid);
                                }
                            } else if (readLen == -1 || readLen == -5 || readLen == -6) {
                                STMS_WARN("Connection to client {} closed forcefully!", lambUUid);
                                lambCli->doShutdown = false;

                                std::lock_guard<std::mutex> lg(clientsMtx);
                                deadClients.push(lambUUid);
                            } else if (readLen == -999) {
                                STMS_WARN("Client {} kicked for: Unknown error", lambUUid);

                                std::lock_guard<std::mutex> lg(clientsMtx);
                                deadClients.push(lambUUid);
                                retryRead = false;
                            } else {
                                STMS_WARN("Client {} SSL_read failed for the reason above!", lambUUid);
                                // Retry.
                            }
                        }
                        lambCli->isReading = false;
                        return nullptr;
                    }, nullptr, 8);
                }
            }
        }

        while (!deadClients.empty()) {
            std::string cliUuid = deadClients.front();
            deadClients.pop();

            pPool->submitTask([=, capStr = std::string(clients[cliUuid]->addrStr), this](void *in) -> void * {
                disconnectCallback(cliUuid, capStr);
                return nullptr;
            }, nullptr, 8);

            STMS_INFO("Client {} at {} disconnected!", cliUuid, clients[cliUuid]->addrStr);
            clients.erase(cliUuid);
        }

        return isRunning;
    }

    bool DTLSServer::tryAddr(addrinfo *addr, int num) {
        int on = 1;

        addrStr = getAddrStr(addr->ai_addr);
        STMS_INFO("Candidate {}: IPv{} {}", num, addr->ai_family == AF_INET6 ? 6 : 4, addrStr);

        if (serverSock != 0) {
            bool runningPreserve = isRunning;
            stop();
            isRunning = runningPreserve;
        }

        serverSock = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
        if (serverSock == -1) {
            STMS_INFO("Candidate {}: Unable to create socket: {}", num, strerror(errno));
            return false;
        }

        if (fcntl(serverSock, F_SETFL, O_NONBLOCK) == -1) {
            STMS_INFO("Candidate {}: Failed to set socket to non-blocking: {}", num, strerror(errno));
        }

        if (addr->ai_family == AF_INET6) {
            if (setsockopt(serverSock, IPPROTO_IPV6, IPV6_V6ONLY, &on, sizeof(int)) == -1) {
                STMS_INFO("Candidate {}: Failed to setsockopt to allow IPv4 connections: {}", num, strerror(errno));
            }
        }

        if (setsockopt(serverSock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(int)) == -1) {
            STMS_INFO(
                    "Candidate {}: Failed to setscokopt to reuse address (this may result in 'Socket in Use' errors): {}",
                    num, strerror(errno));
        }

        if (setsockopt(serverSock, SOL_SOCKET, SO_REUSEPORT, &on, sizeof(int)) == -1) {
            STMS_INFO(
                    "Candidate {}: Failed to setsockopt to reuse port (this may result in 'Socket in Use' errors): {}",
                    num, strerror(errno));
        }

        if (bind(serverSock, addr->ai_addr, addr->ai_addrlen) == -1) {
            STMS_INFO("Candidate {}: Unable to bind socket: {}", num, strerror(errno));
            return false;
        }

        pAddr = addr;
        STMS_INFO("Candidate {} is viable and active. However, it is not necessarily preferred.", num);
        return true;
    }

    int DTLSServer::send(const std::string &clientUuid, const uint8_t *msg, int msgLen) {
        std::lock_guard<std::mutex> lg(clientsMtx);
        if (clients.find(clientUuid) == clients.end()) {
            STMS_WARN("Failed to send! Client with UUID {} does not exist!", clientUuid);
            return 0;
        }

        int ret = -3;

        while (ret == -3) {
            ret = SSL_write(clients[clientUuid]->pSsl, msg, msgLen);
            ret = handleSslGetErr(clients[clientUuid]->pSsl, ret);

            if (ret == -3) {
                STMS_WARN("send() failed with WANT_WRITE! Retrying!");
            } else if (ret <= 0) {
                STMS_WARN("send() failed with error above! Aborting write!");
            }
        }

        if (ret == -1 || ret == -5 || ret == -6) {
            STMS_WARN("Connection to client {} at {} closed forcefully!", clientUuid, clients[clientUuid]->addrStr);
            clients[clientUuid]->doShutdown = false;

            deadClients.push(clientUuid);
        } else if (ret == -999) {
            STMS_WARN("Kicking client {} at {} for: Unknown error", clientUuid, clients[clientUuid]->addrStr);

            deadClients.push(clientUuid);
        } else if (ret < 1) {
            STMS_WARN("SSL_write failed for the reason above!");
        }

        return ret;
    }

    DTLSClientRepresentation::~DTLSClientRepresentation() {
        shutdownClient();
    }

    void DTLSClientRepresentation::shutdownClient() const {
        if (doShutdown && pSsl != nullptr) {
            int shutdownRet = 0;
            while (shutdownRet == 0) {
                shutdownRet = SSL_shutdown(pSsl);
                if (shutdownRet < 0) {
                    handleSslGetErr(pSsl, shutdownRet);
                    flushSSLErrors();
                    STMS_WARN("Error in SSL_shutdown (see above)!");
                    break;
                } else if (shutdownRet == 0) {
                    flushSSLErrors();
                    STMS_INFO("SSL_shutdown needs to be recalled! Retrying...");
                }
            }
        }
        SSL_free(pSsl);
        // No need to free BIO since it is bound to the SSL object and freed when the SSL object is freed.
        BIO_ADDR_free(pBioAddr);

        delete pSockAddr;  // No need to check this, as `delete nullptr` has no effect
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
