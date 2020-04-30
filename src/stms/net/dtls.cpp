//
// Created by grant on 4/22/20.
//

#include "stms/net/dtls.hpp"
#include "stms/util.hpp"
#include "stms/logging.hpp"

#include <unistd.h>
#include <unordered_map>
#include <sys/socket.h>
#include <arpa/inet.h>

#include <fcntl.h>

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

    unsigned long handleSSLError() {
        unsigned long ret = ERR_get_error();
        if (ret != 0) {
            char errStr[256]; // Min length specified by man pages for ERR_error_string_n()
            ERR_error_string_n(ret, errStr, 256);
            STMS_WARN("[** OPENSSL ERROR **]: {}", errStr);
        }
        return ret;
    }

    static uint8_t *hashSSL(SSL *ssl, unsigned *len) {
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


        BIO_ADDR *peer{};
        // TODO: Find documentation for this mystery macro. Is peer really a BIO_ADDR? Stackoverflow this
        BIO_dgram_get_peer(SSL_get_rbio(ssl), &peer);

        size_t addrLen = INET6_ADDRSTRLEN;
        HashAddr addr{};
        addr.port = BIO_ADDR_rawport(peer);
        BIO_ADDR_rawaddress(peer, addr.addr, &addrLen);
        addrLen += sizeof(unsigned short);

        auto result = new uint8_t[DTLS1_COOKIE_LENGTH - 1];
        *len = DTLS1_COOKIE_LENGTH - 1;
        // TODO: Should i really be using SHA1?
        HMAC(EVP_sha1(), secretCookie, STMS_DTLS_COOKIE_SECRET_LEN, (uint8_t *) &addr, addrLen, result, len);
        return result;
    }

    static int genCookie(SSL *ssl, unsigned char *cookie, unsigned int *cookieLen) {
        uint8_t *result = hashSSL(ssl, cookieLen);
        memcpy(cookie, result, *cookieLen);
        delete[] result;

        return 1;
    }

    static int verifyCookie(SSL *ssl, const unsigned char *cookie, unsigned int cookieLen) {
        if (!secretCookieInit) {
            STMS_WARN("Cookie received before a cookie was generated! Are you under a DDoS attack?");
            return 0;
        }
        unsigned expectedLen;
        uint8_t *result = hashSSL(ssl, &expectedLen);

        if (cookieLen == expectedLen && memcmp(result, cookie, expectedLen) == 0) {
            delete[] result;
            return 1;
        }
        delete[] result;

        STMS_WARN("Received an invalid cookie! Are you under a DDoS attack?");
        return 0;

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

        SSL_CTX_set_mode(pCtx, SSL_MODE_ASYNC | SSL_MODE_AUTO_RETRY);
        SSL_CTX_clear_mode(pCtx, SSL_MODE_ENABLE_PARTIAL_WRITE);
        SSL_CTX_set_options(pCtx, SSL_OP_COOKIE_EXCHANGE);
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
        isRunning = true;
        int on = 1;

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

        clients.clear();

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
        int on = 1;

        if (!isRunning) {
            return false;
        }

        DTLSClientRepresentation cli{};
        cli.pBio = BIO_new_dgram(serverSock, BIO_NOCLOSE);
        BIO_ctrl(cli.pBio, BIO_CTRL_DGRAM_SET_RECV_TIMEOUT, 0, &timeout);
        cli.pSsl = SSL_new(pCtx);
        cli.pBioAddr = BIO_ADDR_new();
        SSL_set_bio(cli.pSsl, cli.pBio, cli.pBio);

        SSL_set_options(cli.pSsl, SSL_OP_COOKIE_EXCHANGE);
        SSL_clear_options(cli.pSsl, SSL_OP_NO_COMPRESSION);

        int listenStatus = DTLSv1_listen(cli.pSsl, cli.pBioAddr);

        if (listenStatus < 0) {
            STMS_WARN("Fatal error from DTLSv1_listen!");
        } else if (listenStatus >= 1) {
            if (BIO_ADDR_family(cli.pBioAddr) != AF_INET6 && BIO_ADDR_family(cli.pBioAddr) != AF_INET) {
                STMS_INFO("A client tried to connect with an unsupported family! Refusing to connect.");
                goto skipClientConnect;
            }

            if (BIO_ADDR_family(cli.pBioAddr) == AF_INET6) {
                auto *v6Addr = new sockaddr_in6{};
                v6Addr->sin6_family = AF_INET6;
                v6Addr->sin6_port = BIO_ADDR_rawport(cli.pBioAddr);

                cli.sockAddrLen = sizeof(in6_addr);
                BIO_ADDR_rawaddress(cli.pBioAddr, &v6Addr->sin6_addr, &cli.sockAddrLen);
                cli.pSockAddr = reinterpret_cast<sockaddr *>(v6Addr);
            } else {
                auto *v4Addr = new sockaddr_in{};
                v4Addr->sin_family = AF_INET;
                v4Addr->sin_port = BIO_ADDR_rawport(cli.pBioAddr);

                cli.sockAddrLen = sizeof(in_addr);
                BIO_ADDR_rawaddress(cli.pBioAddr, &v4Addr->sin_addr, &cli.sockAddrLen);
                cli.pSockAddr = reinterpret_cast<sockaddr *>(v4Addr);
            }

            cli.addrStr = getAddrStr(cli.pSockAddr);
            STMS_INFO("New client at {} is trying to connect.", cli.addrStr);

            cli.sock = socket(BIO_ADDR_family(cli.pBioAddr), pAddr->ai_socktype, pAddr->ai_protocol);
            if (cli.sock == -1) {
                STMS_INFO("Failed to bind socket for new client: {}. Refusing to connect.", strerror(errno));
                goto skipClientConnect;
            }

            if (fcntl(cli.sock, F_SETFL, O_NONBLOCK) == -1) {
                STMS_INFO("Failed to set socket to non-blocking: {}. Refusing to connect.", strerror(errno));
                goto skipClientConnect;
            }

            if (BIO_ADDR_family(cli.pBioAddr) == AF_INET6) {
                if (setsockopt(cli.sock, IPPROTO_IPV6, IPV6_V6ONLY, &on, sizeof(int)) == -1) {
                    STMS_INFO("Failed to setsockopt to allow IPv4 connections on client socket: {}", strerror(errno));
                }
            }

            if (setsockopt(cli.sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(int)) == -1) {
                STMS_INFO(
                        "Failed to setscokopt to reuse address on client socket (this may result in 'Socket in Use' errors): {}",
                        strerror(errno));
            }

            if (setsockopt(cli.sock, SOL_SOCKET, SO_REUSEPORT, &on, sizeof(int)) == -1) {
                STMS_INFO(
                        "Failed to setsockopt to reuse port on client socket (this may result in 'Socket in Use' errors): {}",
                        strerror(errno));
            }

            if (bind(cli.sock, pAddr->ai_addr, pAddr->ai_addrlen) == -1) {
                STMS_INFO("Failed to bind client socket: {}. Refusing to connect!", strerror(errno));
                goto skipClientConnect;
            }

            if (connect(cli.sock, cli.pSockAddr, cli.sockAddrLen) == -1) {
                STMS_INFO("Failed to connect client socket: {}. Refusing to connect!", strerror(errno));
            }

            BIO_set_fd(SSL_get_rbio(cli.pSsl), cli.sock, BIO_NOCLOSE);
            BIO_ctrl(SSL_get_rbio(cli.pSsl), BIO_CTRL_DGRAM_SET_CONNECTED, 0, cli.pSockAddr);

            bool doHandshake = true;
            while (doHandshake) {
                int handshakeStatus = SSL_accept(cli.pSsl);
                if (handshakeStatus == 1) {
                    doHandshake = false;
                }
                if (handshakeStatus == 0) {
                    STMS_INFO("Handshake failed with non-fatal error. Retrying.");
                }
                if (handshakeStatus < 0) {
                    STMS_WARN("Fatal DTLS Handshake error! Refusing to connect.");
                    goto skipClientConnect;
                }
            }

            BIO_ctrl(SSL_get_rbio(cli.pSsl), BIO_CTRL_DGRAM_SET_RECV_TIMEOUT, 0, &timeout);

            std::string uuid = stms::genUUID4().getStr();

            char certName[certAndCipherLen];
            char cipherName[certAndCipherLen];
            X509_NAME_oneline(X509_get_subject_name(SSL_get_certificate(cli.pSsl)), certName, certAndCipherLen);
            SSL_CIPHER_description(SSL_get_current_cipher(cli.pSsl), cipherName, certAndCipherLen);

            STMS_INFO("Client (addr='{}', uuid='{}') completed DTLS handshake and connected successfully!",
                      cli.addrStr, uuid);
            STMS_INFO("Client (addr='{}', uuid='{}') has cert of {}", cli.addrStr, uuid, certName);
            STMS_INFO("Client (addr='{}', uuid='{}') is using cipher {}", cli.addrStr, uuid, cipherName);
            STMS_INFO("Client (addr='{}', uuid='{}') is using compression {} and expansion {}", cli.addrStr, uuid,
                      SSL_COMP_get_name(SSL_get_current_compression(cli.pSsl)),
                      SSL_COMP_get_name(SSL_get_current_expansion(cli.pSsl)));

            clients[uuid] = std::move(cli);
        }

        skipClientConnect:  // Please forgive me for using goto

        // We must use this as modifying `clients` while we are looping through is a TERRIBLE idea
        std::vector<std::string> deadClients;

        for (auto &client : clients) {
            if ((SSL_get_shutdown(client.second.pSsl) & SSL_RECEIVED_SHUTDOWN) ||
                client.second.timeouts >= STMS_DTLS_MAX_TIMEOUTS) {
                deadClients.emplace_back(client.first);
            } else {
                bool retryRead = true;
                while (retryRead) {
                    uint8_t recvBuf[STMS_DTLS_MAX_RECV_LEN];
                    int readLen = SSL_read(client.second.pSsl, recvBuf, STMS_DTLS_MAX_RECV_LEN);
                    readLen = handleSslGetErr(client.second.pSsl, readLen);

                    retryRead = false;
                    if (readLen > 0) {
                        recvCallback(recvBuf, readLen);
                    } else if (readLen == -2) {
                        if (BIO_ctrl(SSL_get_rbio(client.second.pSsl), BIO_CTRL_DGRAM_GET_RECV_TIMER_EXP, 0, nullptr)) {
                            client.second.timeouts++;
                            STMS_INFO("SSL_read() timed out for client {}", client.first);
                        } else {
                            retryRead = true;
                        }
                    } else if (readLen == -1 || readLen == -5) {
                        STMS_WARN("Connection to client {} closed forcefully!", client.first);
                        client.second.doShutdown = false;
                        deadClients.emplace_back(client.first);
                    } else if (readLen == -999) {
                        STMS_WARN("Client {} kicked for: Unknown error", client.first);
                        deadClients.emplace_back(client.first);
                    }
                }
            }
        }

        for (const auto &cliUuid : deadClients) {
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

    int DTLSServer::send(const std::string &clientUuid, char *msg, std::size_t msgLen) {
        if (clients.find(clientUuid) == clients.end()) {
            STMS_INFO("Failed to send! Client with UUID {} does not exist!", clientUuid);
            return 0;
        }

        int ret = SSL_write(clients[clientUuid].pSsl, msg, msgLen);

        ret = handleSslGetErr(clients[clientUuid].pSsl, ret);

        if (ret == -1 || ret == -5) {
            STMS_WARN("Connection to client {} closed forcefully!", clientUuid);
            clients[clientUuid].doShutdown = false;
            clients.erase(clientUuid);
        }

        if (ret == -999) {
            STMS_WARN("Kicking client {} for: Unknown error", clientUuid);
            clients.erase(clientUuid);
        }

        return ret;
    }

    int DTLSServer::handleSslGetErr(SSL *ssl, int ret) {
        switch (SSL_get_error(ssl, ret)) {
            case SSL_ERROR_NONE: {
                return ret;
            }
            case SSL_ERROR_ZERO_RETURN: {
                STMS_WARN("Tried to preform SSL IO, but peer has closed the connection! Don't try to read more data!");
                return -6;
            }
            case SSL_ERROR_WANT_WRITE: {
                STMS_WARN("Unable to complete OpenSSL call: Want Write. Please retry. (Auto Retry is on!)");
                return -3;
            }
            case SSL_ERROR_WANT_READ: {
                STMS_WARN("Unable to complete OpenSSL call: Want Read. Please retry. (Auto Retry is on!)");
                return -2;
            }
            case SSL_ERROR_SYSCALL: {
                STMS_WARN("System Error from OpenSSL call: {}", strerror(errno));
                flushSSLErrors();
                return -5;
            }
            case SSL_ERROR_SSL: {
                STMS_WARN("Fatal OpenSSL error occurred!");
                flushSSLErrors();
                return -1;
            }
            case SSL_ERROR_WANT_CONNECT: {
                STMS_WARN("Unable to complete OpenSSL call: SSL hasn't been connected! Retry later!");
                return -7;
            }
            case SSL_ERROR_WANT_ACCEPT: {
                STMS_WARN("Unable to complete OpenSSL call: SSL hasn't been accepted! Retry later!");
                return -8;
            }
            case SSL_ERROR_WANT_X509_LOOKUP: {
                STMS_WARN("The X509 Lookup callback asked to be recalled! Retry later!");
                return -4;
            }
            case SSL_ERROR_WANT_ASYNC: {
                STMS_WARN("Cannot complete OpenSSL call: Async operation in progress. Retry later!");
                return -9;
            }
            case SSL_ERROR_WANT_ASYNC_JOB: {
                STMS_WARN("Cannot complete OpenSSL call: Async thread pool is overloaded! Retry later!");
                return -10;
            }
            case SSL_ERROR_WANT_CLIENT_HELLO_CB: {
                STMS_WARN("ClientHello callback asked to be recalled! Retry Later!");
                return -11;
            }
            default: {
                STMS_WARN("Got an Undefined error from `SSL_get_error()`! This should be impossible!");
                return -999;
            }
        }
    }

    std::string getAddrStr(sockaddr *addr) {
        if (addr->sa_family == AF_INET) {
            auto *v4Addr = reinterpret_cast<sockaddr_in *>(addr);
            char ipStr[INET_ADDRSTRLEN];
            inet_ntop(addr->sa_family, &(v4Addr->sin_addr), ipStr, INET_ADDRSTRLEN);
            return std::string(ipStr) + ":" + std::to_string(ntohs(v4Addr->sin_port));
        } else if (addr->sa_family == AF_INET6) {
            auto *v6Addr = reinterpret_cast<sockaddr_in6 *>(addr);
            char ipStr[INET6_ADDRSTRLEN];
            inet_ntop(addr->sa_family, &(v6Addr->sin6_addr), ipStr, INET6_ADDRSTRLEN);
            return std::string(ipStr) + ":" + std::to_string(ntohs(v6Addr->sin6_port));
        }
        return "[ERR: BAD FAM]";
    }

    DTLSClientRepresentation::~DTLSClientRepresentation() {
        if (doShutdown) {
            SSL_shutdown(pSsl);
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

        addrStr = rhs.addrStr;
        pBioAddr = rhs.pBioAddr;
        pSockAddr = rhs.pSockAddr;
        sockAddrLen = rhs.sockAddrLen;
        pBio = rhs.pBio;
        pSsl = rhs.pSsl;
        sock = rhs.sock;
        timeouts = rhs.timeouts;
        doShutdown = rhs.doShutdown;

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
