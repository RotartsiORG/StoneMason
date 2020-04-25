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


#include "openssl/ssl.h"
#include "openssl/rand.h"

namespace stms::net {

    static bool secretCookieInit = false;
    static uint8_t secretCookie[STMS_DTLS_COOKIE_SECRET_LEN];

    struct HashAddr {
        // int fam;
        unsigned short port;
        uint8_t addr[INET6_ADDRSTRLEN];
    };

    static int verifyCert(int ok, X509_STORE_CTX *ctx) {
        // TODO: Ask user if they trust this cert.
        return 1; // Hardcoded "cert ok"
    }

    unsigned long handleSSLError() {
        unsigned long ret = ERR_get_error();
        if (ret != 0) {
            STMS_WARN("[** OPENSSL ERROR **]: {}", ret);
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
        BIO_dgram_get_peer(SSL_get_rbio(ssl),
                           &peer); // TODO: Find documentation for this mystery macro. Is peer really a BIO_ADDR? Stackoverflow this

        size_t addrLen = INET6_ADDRSTRLEN;
        HashAddr addr{};
        // addr.fam = BIO_ADDR_family(peer); // TODO: Do i need this field?
        addr.port = BIO_ADDR_rawport(peer);
        BIO_ADDR_rawaddress(peer, addr.addr, &addrLen);
        addrLen += sizeof(unsigned short);  // + sizeof(int);

        auto result = new uint8_t[DTLS1_COOKIE_LENGTH - 1];
        *len = DTLS1_COOKIE_LENGTH - 1;
        HMAC(EVP_sha1(), secretCookie, STMS_DTLS_COOKIE_SECRET_LEN, (uint8_t *) &addr, addrLen, result,
             len);  // TODO: Should i really be using SHA1?
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
            return 0;
        }
        unsigned expectedLen;
        uint8_t *result = hashSSL(ssl, &expectedLen);

        if (cookieLen == expectedLen && memcmp(result, cookie, expectedLen) == 0) {
            delete[] result;
            return 1;
        }
        delete[] result;

        STMS_WARN("Recieved an invalid cookie! Are you under a DDoS attack?");
        return 0;

    }

//    static void clientThreadWorker(DTLSServer *server, const std::string &uuid) {
//#define __SDC server->clients[uuid]
//        if (__SDC.addr.sockStore.ss_family != AF_INET6 && !server->IPv4Enabled) { // ip46
//            STMS_INFO(
//                    "Client tried to connect using an internet layer protocol other than IPv4! Refusing connection.");
//            goto clientClean;
//        }
//
//        __SDC.sock = socket(AF_INET6, SOCK_DGRAM, 0); // ip46
//        if (__SDC.sock == -1) {
//            STMS_INFO("Unable to create client socket (Errno: {})", strerror(errno));
//            goto clientClean;
//        }
//
//        if (bind(__SDC.sock, reinterpret_cast<const sockaddr *>(&server->serverAddr), sizeof(server->serverAddr)) ==
//            -1) {
//            STMS_INFO("Unable to bind client socket: (Errno: {})", strerror(errno));
//            goto clientClean;
//        }
//        if (connect(__SDC.sock, reinterpret_cast<const sockaddr *>(&__SDC.addr.sock6), sizeof(__SDC.addr.sock6)) ==
//            -1) { // ip46
//            STMS_INFO("Unable to connect client socket: (Errno: {})", strerror(errno));
//            goto clientClean;
//        }
//
//        __SDC.bio = SSL_get_rbio(__SDC.ssl);
//        BIO_set_fd(__SDC.bio, __SDC.sock, BIO_NOCLOSE);
//        BIO_ctrl(__SDC.bio, BIO_CTRL_DGRAM_SET_CONNECTED, 0, &__SDC.addr.sock6); // ip46
//
//        int err;
//        do { err = SSL_accept(__SDC.ssl); } while (err == 0);
//
//        // Loop here
//
//        clientClean:
//        SSL_free(__SDC.ssl);
//        BIO_free(__SDC.bio);
//        close(__SDC.sock);
//        __SDC.thread.detach();
//        server->clients.erase(uuid);
//#undef __SDC
//    }
//

    DTLSServer::DTLSServer(stms::ThreadPool *pool, const std::string &addr, bool preferV6, const std::string &port,
                           const std::string &certPem,
                           const std::string &keyPem) : pool(pool) {
        ctx = SSL_CTX_new(DTLS_server_method());
        SSL_CTX_use_certificate_chain_file(ctx, certPem.c_str());
        SSL_CTX_use_PrivateKey_file(ctx, keyPem.c_str(), SSL_FILETYPE_PEM);

        SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, verifyCert);
        SSL_CTX_set_cookie_generate_cb(ctx, genCookie);
        SSL_CTX_set_cookie_verify_cb(ctx, verifyCookie);

        addrinfo hints{};
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_DGRAM;
        hints.ai_protocol = IPPROTO_UDP;
        if (addr == "any") {
            hints.ai_flags = AI_PASSIVE;
        }

        addrinfo *res;
        int lookupStatus = getaddrinfo(addr == "any" ? nullptr : addr.c_str(), port.c_str(), &hints, &res);
        if (lookupStatus != 0) {
            STMS_INFO("Failed to resolve ip address of {}:{} (ERRNO: {})", addr, port, gai_strerror(lookupStatus));
        }

        bool found = false;
        for (addrinfo *i = res; i != nullptr; i = i->ai_next) {
            if (i->ai_family == AF_INET6) {
                if (!found && preferV6) {
                    servAddr = *i;
                    v6 = false;
                } else if (!preferV6) {
                    servAddr = *i;
                    v6 = false;
                    found = true;
                }
            } else if (i->ai_family == AF_INET) {
                if (!found && !preferV6) {
                    v6 = true;
                    servAddr = *i;
                } else if (preferV6) {
                    v6 = true;
                    servAddr = *i;
                    found = true;
                }
            } else {
                STMS_INFO("No available IPv6 or IPv4 addresses can be resolved for DTLS server!");
            }
        }

        freeaddrinfo(res);

        if (!v6) {
            auto *v4Addr = reinterpret_cast<sockaddr_in *>(servAddr.ai_addr);
            char ipStr[INET_ADDRSTRLEN];
            inet_ntop(servAddr.ai_family, &(v4Addr->sin_addr), ipStr, INET_ADDRSTRLEN);
            servAddrStr = std::string(ipStr) + ":" + std::to_string(ntohs(v4Addr->sin_port));
        } else {
            auto *v6Addr = reinterpret_cast<sockaddr_in6 *>(servAddr.ai_addr);
            char ipStr[INET6_ADDRSTRLEN];
            inet_ntop(servAddr.ai_family, &(v6Addr->sin6_addr), ipStr, INET_ADDRSTRLEN);
            servAddrStr = std::string(ipStr) + ":" + std::to_string(ntohs(v6Addr->sin6_port));
        }

        STMS_INFO("Successfully resolved hostname to IPv{} address: {}", v6 ? 6 : 4, servAddrStr);
    }

    DTLSServer::~DTLSServer() {
        SSL_CTX_free(ctx);
    }

    DTLSServer &DTLSServer::operator=(DTLSServer &&rhs) noexcept {
        return *this;
    }

    DTLSServer::DTLSServer(DTLSServer &&rhs) noexcept {
        *this = std::move(rhs);
    }

    void DTLSServer::start() {
        isRunning = true;
        int on = 1;

        serverSock = socket(servAddr.ai_family, SOCK_DGRAM, servAddr.ai_protocol);  // ip46
        if (serverSock == -1) {
            STMS_INFO("[DTLSServer.start()]: Unable to create socket: {}", strerror(errno));
            stop();
        }

        if (fcntl(serverSock, F_SETFL, O_NONBLOCK) == -1) {
            STMS_INFO("[DTLSServer.start()]: Failed to set socket to non-blocking: {}", strerror(errno));
        }

        if (v6) {
            if (setsockopt(serverSock, IPPROTO_IPV6, IPV6_V6ONLY, &on, sizeof(int)) == -1) {
                STMS_INFO("[DTLSServer.start()]: Failed to setsockopt to allow IPv4 connections: {}", strerror(errno));
            }
        }

        if (setsockopt(serverSock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(int)) == -1) {
            STMS_INFO(
                    "[DTLSServer.start()]: Failed to setscokopt to reuse address (this may result in 'Socket in Use' errors): {}",
                    strerror(errno));
        }

        if (setsockopt(serverSock, SOL_SOCKET, SO_REUSEPORT, &on, sizeof(int)) == -1) {
            STMS_INFO(
                    "[DTLSServer.start()]: Failed to setsockopt to reuse port (this may result in 'Socket in Use' errors): {}",
                    strerror(errno));
        }

        if (bind(serverSock, servAddr.ai_addr, servAddr.ai_addrlen) == -1) {
            STMS_INFO("[DTLSServer.start()]: Unable to bind socket: {}", strerror(errno));
            stop();
        }

        STMS_INFO("DTLS Server started on {}", servAddrStr);
    }

    void DTLSServer::stop() {
        isRunning = false;
        if (shutdown(serverSock, 0) == -1) {
            STMS_INFO("[DTLSServer.stop()]: Failed to shutdown server socket: {}", strerror(errno));
        }
        if (close(serverSock) == -1) {

        }
        serverSock = 0;
        STMS_INFO("[DTLSServer.stop()]: DTLS server stopped. Resources freed.");
    }

    bool DTLSServer::tick() {
        if (!isRunning) {
            return false;
        }

        DTLSClientRepresentation cli{};
        cli.bio = BIO_new_dgram(serverSock, BIO_NOCLOSE);
        cli.ssl = SSL_new(ctx);
        SSL_set_bio(cli.ssl, cli.bio, cli.bio);

        SSL_set_options(cli.ssl, SSL_OP_COOKIE_EXCHANGE);

        int listenStatus = DTLSv1_listen(cli.ssl, cli.addr);
        if (listenStatus < 0) {
            STMS_INFO("DTLSv1_listen failed!");
        }

        return isRunning;
    }
}
