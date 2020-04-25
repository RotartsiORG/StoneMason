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

    static bool secretCookieInit = false;
    static uint8_t secretCookie[STMS_DTLS_COOKIE_SECRET_LEN];

    struct HashAddr {
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
        BIO_dgram_get_peer(SSL_get_rbio(ssl),
                           &peer); // TODO: Find documentation for this mystery macro. Is peer really a BIO_ADDR? Stackoverflow this

        size_t addrLen = INET6_ADDRSTRLEN;
        HashAddr addr{};
        addr.port = BIO_ADDR_rawport(peer);
        BIO_ADDR_rawaddress(peer, addr.addr, &addrLen);
        addrLen += sizeof(unsigned short);

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

    DTLSServer::DTLSServer(stms::ThreadPool *pool, const std::string &addr, bool preferV6, const std::string &port,
                           const std::string &certPem,
                           const std::string &keyPem) : wantV6(preferV6), pool(pool) {
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

        STMS_INFO("Creating new DTLS server to be hosted on {}:{}", addr, port);
        int lookupStatus = getaddrinfo(addr == "any" ? nullptr : addr.c_str(), port.c_str(), &hints, &servAddr);
        if (lookupStatus != 0) {
            STMS_INFO("Failed to resolve ip address of {}:{} (ERRNO: {})", addr, port, gai_strerror(lookupStatus));
        }
    }

    DTLSServer::~DTLSServer() {
        stop();
        SSL_CTX_free(ctx);
        freeaddrinfo(servAddr);
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

        int i = 0;
        for (addrinfo *p = servAddr; p != nullptr; p = p->ai_next) {
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

        STMS_WARN("No IP addresses resolved from supplied addr and port can be used to host the DTLS Server!");
        isRunning = false;
    }

    void DTLSServer::stop() {
        isRunning = false;
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
        cli.bio = BIO_new_dgram(serverSock, BIO_NOCLOSE);
        cli.ssl = SSL_new(ctx);
        cli.addr = BIO_ADDR_new();
        SSL_set_bio(cli.ssl, cli.bio, cli.bio);

        SSL_set_options(cli.ssl, SSL_OP_COOKIE_EXCHANGE);

        int listenStatus = DTLSv1_listen(cli.ssl, cli.addr);

        if (listenStatus < 0) {
            STMS_WARN("Fatal error from DTLSv1_listen!");
        } else if (listenStatus >= 1) {
            if (BIO_ADDR_family(cli.addr) != AF_INET6 && BIO_ADDR_family(cli.addr) != AF_INET) {
                STMS_INFO("A client tried to connect with an unsupported family! Refusing to connect.");
                goto skipClientConnect;
            }

            if (BIO_ADDR_family(cli.addr) == AF_INET6) {
                auto *v6Addr = new sockaddr_in6{};
                v6Addr->sin6_family = AF_INET6;
                v6Addr->sin6_port = BIO_ADDR_rawport(cli.addr);

                cli.sockAddrLen = sizeof(in6_addr);
                BIO_ADDR_rawaddress(cli.addr, &v6Addr->sin6_addr, &cli.sockAddrLen);
                cli.sockAddr = reinterpret_cast<sockaddr *>(v6Addr);
            } else {
                auto *v4Addr = new sockaddr_in{};
                v4Addr->sin_family = AF_INET;
                v4Addr->sin_port = BIO_ADDR_rawport(cli.addr);

                cli.sockAddrLen = sizeof(in_addr);
                BIO_ADDR_rawaddress(cli.addr, &v4Addr->sin_addr, &cli.sockAddrLen);
                cli.sockAddr = reinterpret_cast<sockaddr *>(v4Addr);
            }

            cli.addrStr = getAddrStr(cli.sockAddr);
            STMS_INFO("New client at {} is trying to connect.", cli.addrStr);

            cli.sock = socket(BIO_ADDR_family(cli.addr), activeAddr->ai_socktype, activeAddr->ai_protocol);
            if (cli.sock == -1) {
                STMS_INFO("Failed to bind socket for new client: {}. Refusing to connect.", strerror(errno));
                goto skipClientConnect;
            }

            if (fcntl(cli.sock, F_SETFL, O_NONBLOCK) == -1) {
                STMS_INFO("Failed to set socket to non-blocking: {}. Refusing to connect.", strerror(errno));
                goto skipClientConnect;
            }

            if (BIO_ADDR_family(cli.addr) == AF_INET6) {
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

            if (bind(cli.sock, activeAddr->ai_addr, activeAddr->ai_addrlen) == -1) {
                STMS_INFO("Failed to bind client socket: {}. Refusing to connect!", strerror(errno));
                goto skipClientConnect;
            }

            if (connect(cli.sock, cli.sockAddr, cli.sockAddrLen) == -1) {
                STMS_INFO("Failed to connect client socket: {}. Refusing to connect!", strerror(errno));
            }

            BIO_set_fd(SSL_get_rbio(cli.ssl), cli.sock, BIO_NOCLOSE);
            BIO_ctrl(SSL_get_rbio(cli.ssl), BIO_CTRL_DGRAM_SET_CONNECTED, 0, cli.sockAddr);

            bool doHandshake = true;
            while (doHandshake) {
                int handshakeStatus = SSL_accept(cli.ssl);
                if (handshakeStatus == 1) {
                    STMS_INFO("Client completed DTLS handshake successfully!");
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

            std::string uuid = stms::genUUID4().getStr();
            clients[uuid] = std::move(cli);
        }

        skipClientConnect:  // Please forgive me for using goto

        for (auto client = clients.begin(); client != clients.end(); ++client) {

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

        activeAddr = addr;
        STMS_INFO("Candidate {} is viable and being used.", num);
        return true;
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
        if (ssl != nullptr) {
            SSL_free(ssl);
        }
        // No need to free BIO since it is bound to the SSL object and freed when the SSL object is freed.
        if (addr != nullptr) {
            BIO_ADDR_free(addr);
        }

        delete sockAddr;  // No need to check this, as `delete nullptr` has no effect
    }

    DTLSClientRepresentation &DTLSClientRepresentation::operator=(DTLSClientRepresentation &&rhs) noexcept {
        if (&rhs == this) {
            return *this;
        }

        addrStr = rhs.addrStr;
        addr = rhs.addr;
        sockAddr = rhs.sockAddr;
        sockAddrLen = rhs.sockAddrLen;
        bio = rhs.bio;
        ssl = rhs.ssl;
        sock = rhs.sock;

        rhs.sock = 0;
        rhs.ssl = nullptr;
        rhs.bio = nullptr;
        rhs.sockAddr = nullptr;
        rhs.addr = nullptr;

        return *this;
    }

    DTLSClientRepresentation::DTLSClientRepresentation(DTLSClientRepresentation &&rhs) noexcept {
        *this = std::move(rhs);
    }
}
