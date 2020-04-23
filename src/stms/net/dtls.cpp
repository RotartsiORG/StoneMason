//
// Created by grant on 4/22/20.
//

#include "stms/net/dtls.hpp"
#include "stms/util.hpp"
#include "stms/logging.hpp"
#include <unordered_map>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "openssl/ssl.h"
#include "openssl/rand.h"

namespace stms::net {

    static bool secretCookieInit = false;
    static uint8_t secretCookie[STMS_DTLS_COOKIE_SECRET_LEN];

    static int verifyCert(int ok, X509_STORE_CTX *ctx) {
        return 1; // Hardcoded "cert ok"
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


        BioAddrType peer{};
        BIO_dgram_get_peer(SSL_get_rbio(ssl), &peer);

        auto result = new uint8_t[DTLS1_COOKIE_LENGTH - 1];
        *len = DTLS1_COOKIE_LENGTH - 1;
        HMAC(EVP_sha3_224(), secretCookie, STMS_DTLS_COOKIE_SECRET_LEN, reinterpret_cast<uint8_t *>(&peer),
             sizeof(BioAddrType), result, len);
        return result;
    }

    // TODO: Hash before sending
    static int genCookie(SSL *ssl, unsigned char *cookie, unsigned int *cookieLen) {
        uint8_t *result = hashSSL(ssl, cookieLen);
        memcpy(cookie, result, *cookieLen);
        delete[] result;

        return 1;
    }

    // TODO: Hashes
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

    static void mainThreadWorker(DTLSServer *server) {
        bool on = true;

        server->serverSock = socket(AF_INET6, SOCK_DGRAM, 0);
        //setsockopt(server->serverSock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(bool));
        bind(server->serverSock, reinterpret_cast<const sockaddr *>(&server->serverAddr), sizeof(sockaddr_in6));

        while (server->isRunning) {
            BIO *bio = BIO_new_dgram(server->serverSock, BIO_NOCLOSE);
            SSL *ssl = SSL_new(server->ctx);
            SSL_set_bio(ssl, bio, bio);

            SSL_set_options(ssl, SSL_OP_COOKIE_EXCHANGE);

            BioAddrType cliAddr{};
            while (!DTLSv1_listen(ssl, reinterpret_cast<BIO_ADDR *>(&cliAddr)));
            if (cliAddr.sockStore.ss_family != AF_INET6) {
                STMS_INFO(
                        "Client tried to connect using an internet layer protocol other than IPv6! Refusing connection.");
                continue;
            }

            int clientSock = socket(AF_INET6, SOCK_DGRAM, 0);
            bind(clientSock, reinterpret_cast<const sockaddr *>(&server->serverAddr), sizeof(sockaddr_in6));
            connect(clientSock, reinterpret_cast<const sockaddr *>(&cliAddr.sock6), sizeof(sockaddr_in6));

            BIO *clientBio = SSL_get_rbio(ssl);
            BIO_set_fd(clientBio, clientSock, BIO_NOCLOSE);
            BIO_ctrl(clientBio, BIO_CTRL_DGRAM_SET_CONNECTED, 0, &cliAddr.sock6);

            int err;
            do { err = SSL_accept(ssl); } while (err == 0);

        }
    }

    DTLSServer::DTLSServer(const std::string &ipAddr, unsigned int port, const std::string &certPem,
                           const std::string &keyPem) {
        ctx = SSL_CTX_new(DTLS_server_method());
        SSL_CTX_use_certificate_chain_file(ctx, certPem.c_str());
        SSL_CTX_use_PrivateKey_file(ctx, keyPem.c_str(), SSL_FILETYPE_PEM);

        SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, verifyCert);
        SSL_CTX_set_cookie_generate_cb(ctx, genCookie);
        SSL_CTX_set_cookie_verify_cb(ctx, verifyCookie);

        inet_pton(AF_INET6, ipAddr.c_str(), &serverAddr.sin6_addr);
        serverAddr.sin6_port = port;

    }

    DTLSServer::~DTLSServer() {

    }

    DTLSServer &DTLSServer::operator=(DTLSServer &&rhs) noexcept {
        return *this;
    }

    DTLSServer::DTLSServer(DTLSServer &&rhs) noexcept {
        *this = std::move(rhs);
    }

    void DTLSServer::start() {
        isRunning = true;
        controlThread = std::thread(mainThreadWorker, this);
    }

    void DTLSServer::stop() {
        isRunning = false;
        if (controlThread.joinable()) {
            controlThread.join();
        } else {
            controlThread.detach();
        }
    }
}
