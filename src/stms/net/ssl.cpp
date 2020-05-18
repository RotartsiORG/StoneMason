//
// Created by grant on 4/30/20.
//

#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>

#include "stms/net/ssl.hpp"
#include "stms/logging.hpp"
#include "openssl/rand.h"
#include "openssl/err.h"

namespace stms::net {
    std::string getAddrStr(const sockaddr *const addr) {
        if (addr->sa_family == AF_INET) {
            auto *v4Addr = reinterpret_cast<const sockaddr_in *>(addr);
            char ipStr[INET_ADDRSTRLEN];
            inet_ntop(addr->sa_family, &(v4Addr->sin_addr), ipStr, INET_ADDRSTRLEN);
            return std::string(ipStr) + ":" + std::to_string(ntohs(v4Addr->sin_port));
        } else if (addr->sa_family == AF_INET6) {
            auto *v6Addr = reinterpret_cast<const sockaddr_in6 *>(addr);
            char ipStr[INET6_ADDRSTRLEN];
            inet_ntop(addr->sa_family, &(v6Addr->sin6_addr), ipStr, INET6_ADDRSTRLEN);
            return std::string(ipStr) + ":" + std::to_string(ntohs(v6Addr->sin6_port));
        }
        return "[ERR: BAD FAM]";
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

    int handleSslGetErr(SSL *ssl, int ret) {
        switch (SSL_get_error(ssl, ret)) {
            case SSL_ERROR_NONE: {
                return ret;
            }
            case SSL_ERROR_ZERO_RETURN: {
                STMS_WARN("Tried to preform SSL IO, but peer has closed the connection! Don't try to read more data!");
                flushSSLErrors();
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

    void initOpenSsl() {
        // OpenSSL initialization
        if (OpenSSL_version_num() != OPENSSL_VERSION_NUMBER) {
            STMS_CRITICAL("[* FATAL ERROR *] OpenSSL version mismatch! "
                          "Linked and compiled/included OpenSSL versions are not the same!");
            STMS_CRITICAL("Compiled/Included OpenSSL {} while linked OpenSSL {}",
                          OPENSSL_VERSION_TEXT, OpenSSL_version(OPENSSL_VERSION));
            if (OpenSSL_version_num() >> 20 != OPENSSL_VERSION_NUMBER >> 20) {
                STMS_CRITICAL("OpenSSL major and minor version numbers do not match. Aborting.");
                // TODO: implement
                STMS_CRITICAL("Set `STMS_IGNORE_SSL_MISMATCH` to `true` in `config.hpp` to ignore this error.");
                STMS_CRITICAL("Only do this if you plan on NEVER using OpenSSL functionality.");
                throw std::runtime_error("OpenSSL version mismatch!");
            }
        }

        if (OPENSSL_VERSION_NUMBER < 0x1010102fL) {
            STMS_CRITICAL("{} is outdated and insecure! At least OpenSSL 1.1.1a is required!",
                          OpenSSL_version(OPENSSL_VERSION));
            // TODO: implement
            STMS_CRITICAL("Set `STMS_IGNORE_OLD_SSL` to `true` in `config.hpp` to ignore this error.");
            STMS_CRITICAL("Only do this if you plan on NEVER using OpenSSL functionality.");
            throw std::runtime_error("Outdated OpenSSL!");
        }

        int pollStatus = RAND_poll();
        if (pollStatus != 1 || RAND_status() != 1) {
            STMS_CRITICAL("[* FATAL ERROR *] Unable to seed OpenSSL RNG with enough random data!");
            STMS_CRITICAL("OpenSSL may generate insecure cryptographic keys, and UUID collisions may occur");
            if (!STMS_IGNORE_BAD_RNG) {
                STMS_CRITICAL("Aborting! Set `STMS_IGNORE_BAD_RNG` to ignore this fatal error.");
                STMS_CRITICAL("Only do this if you plan on NEVER using OpenSSL functionality.");
                throw std::runtime_error("Bad OpenSSL RNG!");
            } else {
                STMS_CRITICAL("Using insecure RNG! Only do this if you plan on NEVER using OpenSSL functionality.");
                STMS_CRITICAL("Otherwise, set `STMS_IGNORE_BAD_RNG` to `false` in `config.hpp`");
            }
        }

        SSL_COMP_add_compression_method(0, COMP_zlib());
        STMS_INFO("Initialized {}!", OpenSSL_version(OPENSSL_VERSION));
    }


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


    SSLBase::SSLBase(bool isServ, stms::ThreadPool *pool, const std::string &addr, const std::string &port,
                     bool preferV6,
                     const std::string &certPem, const std::string &keyPem, const std::string &caCert,
                     const std::string &caPath, const std::string &password) : isServ(isServ), wantV6(preferV6),
                                                                               pPool(pool) {
        if (!password.empty()) {
            this->password = new char[password.length()];
            strcpy(this->password, password.c_str());
        } else {
            this->password = new char;
            *this->password = 0;
        }

        pCtx = SSL_CTX_new(isServ ? DTLS_server_method() : DTLS_client_method());

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

        STMS_INFO("DTLS {} to be hosted on {}:{}", isServ ? "server" : "client", addr, port);
        int lookupStatus = getaddrinfo(addr == "any" ? nullptr : addr.c_str(), port.c_str(), &hints, &pAddrCandidates);
        if (lookupStatus != 0) {
            STMS_INFO("Failed to resolve ip address of {}:{} (ERRNO: {})", addr, port, gai_strerror(lookupStatus));
        }
    }

    void SSLBase::stop() {
        if (!isRunning) {
            STMS_WARN("DTLS server/client stop() called when server/client already stopped! Ignoring invocation!");
            return;
        }

        isRunning = false;
        onStop();
        if (sock == 0) {
            STMS_INFO("DTLS server/client stopped. Resources freed. (Skipped socket as fd was 0)");
            return;
        }

        if (shutdown(sock, 0) == -1) {
            STMS_INFO("Failed to shutdown server/client socket: {}", strerror(errno));
        }
        if (close(sock) == -1) {
            STMS_INFO("Failed to close server/client socket: {}", strerror(errno));
        }
        sock = 0;
        STMS_INFO("DTLS server/client stopped. Resources freed.");
    }

    void SSLBase::start() {
        if (isRunning) {
            STMS_WARN("SSL start() called when server or client already started! Ignoring invocation!");
            return;
        }

        if (!pPool->isRunning()) {
            STMS_CRITICAL("DTLSServer started with a stopped thread pool! "
                          "This will result in nothing being proccessed!");
            STMS_CRITICAL("Please explicitly start it before calling start()"
                          " and leave it running until the server/client stops!");
            STMS_CRITICAL("Refusing to start DTLS Server/Client!");
            return;
        }

        int i = 0;
        for (addrinfo *p = pAddrCandidates; p != nullptr; p = p->ai_next) {
            if (p->ai_family == AF_INET6) {
                if (tryAddr(p, i)) {
                    if (wantV6) {
                        STMS_INFO("Using Candidate {} because it is IPv6", i);
                        isRunning = true;
                        onStart();
                        return;
                    }
                }
            } else if (p->ai_family == AF_INET) {
                if (tryAddr(p, i)) {
                    if (!wantV6) {
                        STMS_INFO("Using Candidate {} because it is IPv4", i);
                        isRunning = true;
                        onStart();
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

    bool SSLBase::tryAddr(addrinfo *addr, int num) {
        int on = 1;

        addrStr = getAddrStr(addr->ai_addr);
        STMS_INFO("Candidate {}: IPv{} {}", num, addr->ai_family == AF_INET6 ? 6 : 4, addrStr);

        if (sock != 0) {
            if (shutdown(sock, 0) == -1) {
                STMS_INFO("Failed to shutdown socket: {}", strerror(errno));
            }
            if (close(sock) == -1) {
                STMS_INFO("Failed to close socket: {}", strerror(errno));
            }
            sock = 0;
        }

        sock = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
        if (sock == -1) {
            STMS_INFO("Candidate {}: Unable to create socket: {}", num, strerror(errno));
            return false;
        }

        if (fcntl(sock, F_SETFL, O_NONBLOCK) == -1) {
            STMS_INFO("Candidate {}: Failed to set socket to non-blocking: {}", num, strerror(errno));
        }

        if (addr->ai_family == AF_INET6) {
            if (setsockopt(sock, IPPROTO_IPV6, IPV6_V6ONLY, &on, sizeof(int)) == -1) {
                STMS_INFO("Candidate {}: Failed to setsockopt to allow IPv4 connections: {}", num, strerror(errno));
            }
        }

        if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(int)) == -1) {
            STMS_INFO(
                    "Candidate {}: Failed to setscokopt to reuse address (this may result in 'Socket in Use' errors): {}",
                    num, strerror(errno));
        }

        if (setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, &on, sizeof(int)) == -1) {
            STMS_INFO(
                    "Candidate {}: Failed to setsockopt to reuse port (this may result in 'Socket in Use' errors): {}",
                    num, strerror(errno));
        }

        if (isServ) {
            if (bind(sock, addr->ai_addr, addr->ai_addrlen) == -1) {
                STMS_INFO("Candidate {}: Unable to bind socket: {}", num, strerror(errno));
                return false;
            }
        } else {
            if (connect(sock, addr->ai_addr, addr->ai_addrlen) == -1) {
                STMS_INFO("Candidate {}: Failed to connect socket: {}", num, strerror(errno));
                return false;
            }
        }

        pAddr = addr;
        STMS_INFO("Candidate {} is viable and active. However, it is not necessarily preferred.", num);
        return true;
    }

    SSLBase::~SSLBase() {
        delete[] password;
        SSL_CTX_free(pCtx);
        freeaddrinfo(pAddrCandidates);
    }

    void SSLBase::onStart() {
        // no-op
    }

    bool SSLBase::blockUntilReady(int fd, SSL *ssl, short event) {
        pollfd cliPollFd{};
        cliPollFd.events = event;
        cliPollFd.fd = fd;

        timeval timevalTimeout{};
        DTLSv1_get_timeout(ssl, &timevalTimeout);
        int recvTimeout = static_cast<int>(timevalTimeout.tv_sec * 1000 + timevalTimeout.tv_usec / 1000);
//        recvTimeout = recvTimeout > minIoTimeout ? recvTimeout : minIoTimeout;

        STMS_INFO("DTLS Recv timeout is set for {} ms", recvTimeout);

        if (poll(&cliPollFd, 1, recvTimeout) == 0) {
            STMS_WARN("poll() timed out!");
            DTLSv1_handle_timeout(ssl);
            return false;
        }

        if (!(cliPollFd.revents & event)) {
            STMS_WARN("Desired flags not set in poll()!");
            DTLSv1_handle_timeout(ssl);
            return false;
        }

        return true;
    }

    void SSLBase::onStop() {
        // no-op
    }
}

