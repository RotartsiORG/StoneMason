//
// Created by grant on 4/30/20.
//

#include "stms/net/ssl.hpp"

#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>

#include "stms/stms.hpp"
#include "stms/util/util.hpp"
#include "stms/logging.hpp"

#include "openssl/rand.h"
#include "openssl/err.h"
#include "openssl/conf.h"

namespace stms {
    unsigned long handleSSLError() {
        unsigned long ret = ERR_get_error();
        if (ret != 0) {
            char errStr[256]; // Min length specified by man pages for ERR_error_string_n()
            ERR_error_string_n(ret, errStr, 256);
            STMS_ERROR("[** OPENSSL ERROR **]: {}", errStr);
        }
        return ret;
    }

    int handleSslGetErr(SSL *ssl, int ret) {
        switch (SSL_get_error(ssl, ret)) {
            case SSL_ERROR_NONE: {
                return ret;
            }
            case SSL_ERROR_ZERO_RETURN: {
                STMS_ERROR("Tried to preform SSL IO, but peer has closed the connection! Don't try to read more data!");
                flushSSLErrors();
                throw SSLFatalException();
            }
            case SSL_ERROR_WANT_WRITE: {
                STMS_WARN("Unable to complete OpenSSL call: Want Write. Please retry. (Auto Retry is on!)");
                throw SSLWantWriteException();
            }
            case SSL_ERROR_WANT_READ: {
                STMS_WARN("Unable to complete OpenSSL call: Want Read. Please retry. (Auto Retry is on!)");
                throw SSLWantReadException();
            }
            case SSL_ERROR_SYSCALL: {
                STMS_ERROR("System Error from OpenSSL call: {}", strerror(errno));
                flushSSLErrors();
                throw SSLFatalException();
            }
            case SSL_ERROR_SSL: {
                STMS_ERROR("Fatal OpenSSL error occurred!");
                flushSSLErrors();
                throw SSLFatalException();
            }
            case SSL_ERROR_WANT_CONNECT: {
                STMS_WARN("Unable to complete OpenSSL call: SSL hasn't been connected! Retry later!");
                throw SSLException();
            }
            case SSL_ERROR_WANT_ACCEPT: {
                STMS_WARN("Unable to complete OpenSSL call: SSL hasn't been accepted! Retry later!");
                throw SSLException();
            }
            case SSL_ERROR_WANT_X509_LOOKUP: {
                STMS_WARN("The X509 Lookup callback asked to be recalled! Retry later!");
                throw SSLException();
            }
            case SSL_ERROR_WANT_ASYNC: {
                STMS_WARN("Cannot complete OpenSSL call: Async operation in progress. Retry later!");
                throw SSLException();
            }
            case SSL_ERROR_WANT_ASYNC_JOB: {
                STMS_WARN("Cannot complete OpenSSL call: Async thread pool is overloaded! Retry later!");
                throw SSLException();
            }
            case SSL_ERROR_WANT_CLIENT_HELLO_CB: {
                STMS_WARN("ClientHello callback asked to be recalled! Retry Later!");
                throw SSLException();
            }
            default: {
                STMS_ERROR("Got an Undefined error from `SSL_get_error()`! This should be impossible!");
                throw SSLFatalException();
            }
        }
    }

    void initOpenSsl() {
        // OpenSSL initialization
        if (OpenSSL_version_num() != OPENSSL_VERSION_NUMBER) {
            STMS_FATAL("[* FATAL ERROR *] OpenSSL version mismatch! Compiled with "
                            "{} but linked {}. Aborting!", OPENSSL_VERSION_TEXT, OpenSSL_version(OPENSSL_VERSION));
            STMS_FATAL("Aborting! Disable OpenSSL by setting `STMS_NO_OPENSSL` in `config.hpp` to supress this.");
            throw std::runtime_error("OpenSSL version mismatch!");
        }

        if (OPENSSL_VERSION_NUMBER < 0x10101000L) {
            STMS_FATAL("[* FATAL ERROR *] {} is outdated and insecure!"
                            "At least OpenSSL 1.1.1 is required! Aborting.", OpenSSL_version(OPENSSL_VERSION));
            STMS_FATAL("Aborting! Disable OpenSSL by setting `STMS_NO_OPENSSL` in `config.hpp` to supress this.");
            throw std::runtime_error("Outdated OpenSSL!");
        }

        int pollStatus = RAND_poll();
        if (pollStatus != 1 || RAND_status() != 1) {
            STMS_FATAL("[* FATAL ERROR *] Unable to seed OpenSSL RNG with enough random data! Aborting.");
            STMS_FATAL("OpenSSL may generate insecure cryptographic keys, and UUID collisions may occur");
            STMS_FATAL("Aborting! Disable OpenSSL by setting `STMS_NO_OPENSSL` in `config.hpp` to supress this.");
            throw std::runtime_error("Bad OpenSSL RNG!");
        }

        SSL_COMP_add_compression_method(0, COMP_zlib());
        STMS_INFO("Initialized {}!", OpenSSL_version(OPENSSL_VERSION));

        if (RAND_bytes(getSecretCookie(), sizeof(uint8_t) * secretCookieLen) != 1) {
            STMS_FATAL(
                    "OpenSSL RNG is faulty! Using C++ RNG instead! This may leave you vulnerable to DDoS attacks!");
            std::generate(getSecretCookie(), getSecretCookie() + secretCookieLen, []() {
                return stms::intRand(0, UINT8_MAX);
            });
        }

        fmt::memory_buffer scdump;
        fmt::format_to(scdump, "OpenSSL secret cookie dump (len {}): ", secretCookieLen);
        for (int i = 0; i < secretCookieLen; i++) {
            fmt::format_to(scdump, "{:02x} ", getSecretCookie()[i]);
        }
        *scdump.end() = '\0';
        STMS_INFO(scdump.begin());
    }

    void quitOpenSsl() {
        CONF_modules_unload(1);
    }

    static int getPassword(char *dest, int size, int, void *pass) {
        auto *passStr = reinterpret_cast<std::string *>(pass);
        if (passStr->empty()) {
            dest[0] = '\0';
            return 0;
        }

        if (static_cast<std::string::size_type>(size) < (passStr->size() + 1)) { // Null-terminate
            STMS_ERROR("Provided SSL password too long! Got len={} when max len is {}", passStr->size(), size - 1);
            dest[0] = '\0';
            return 0;
        }

        std::copy(passStr->begin(), passStr->end(), dest);
        dest[passStr->size()] = '\0';
        // THIS WILL PRINT THE CERTIFICATE PASSWORD!!! (it should be fine, as it is only local).
        STMS_TRACE("pass in = {}, pass out = {}", *passStr, dest);
        return passStr->size();
    }

    static int verifyCert(int ok, X509_STORE_CTX *ctx) {
        char name[certAndCipherLen];

        X509 *cert = X509_STORE_CTX_get_current_cert(ctx);
        X509_NAME_oneline(X509_get_subject_name(cert), name, certAndCipherLen);
        if (ok) {
            STMS_INFO("Trusting certificate: {}", name);
        } else {
            STMS_WARN("Rejecting certificate: {}", name);
        }
        return ok;
    }


    _stms_SSLBase::_stms_SSLBase(bool serv, stms::PoolLike *pool, bool udp) : _stms_PlainBase(serv, pool, udp) {

        if (isUdp) {
            pCtx = SSL_CTX_new(isServ ? DTLS_server_method() : DTLS_client_method());
        } else {
            pCtx = SSL_CTX_new(isServ ? TLS_server_method() : TLS_client_method());
        }

        SSL_CTX_set_verify(pCtx, SSL_VERIFY_PEER, verifyCert);

        SSL_CTX_set_mode(pCtx, SSL_MODE_AUTO_RETRY | SSL_MODE_ASYNC);
        SSL_CTX_clear_mode(pCtx, SSL_MODE_ENABLE_PARTIAL_WRITE);
        SSL_CTX_set_options(pCtx, SSL_OP_COOKIE_EXCHANGE | SSL_OP_NO_ANTI_REPLAY);
        SSL_CTX_clear_options(pCtx, SSL_OP_NO_COMPRESSION);
    }

    void _stms_SSLBase::setVerifyMode(int mode) {
        SSL_CTX_set_verify(pCtx, mode, verifyCert);
    }

    _stms_SSLBase::~_stms_SSLBase() {
        SSL_CTX_free(pCtx);
    }

    bool _stms_SSLBase::blockUntilReady(int fd, SSL *ssl, short event) const {
        pollfd cliPollFd{};
        cliPollFd.events = event;
        cliPollFd.fd = fd;

        int recvTimeout;
        if (isUdp) {
            timeval timevalTimeout{};
            DTLSv1_get_timeout(ssl, &timevalTimeout);
            recvTimeout = static_cast<int>(timevalTimeout.tv_sec * 1000 + timevalTimeout.tv_usec / 1000);
        } else {
            recvTimeout = static_cast<int>(timeoutMs);
        }

        if (recvTimeout < minIoTimeout) { recvTimeout = minIoTimeout; }


        recvTimeout = recvTimeout > minIoTimeout ? recvTimeout : minIoTimeout;

        STMS_INFO("Recv timeout is set for {} ms", recvTimeout);

        if (poll(&cliPollFd, 1, recvTimeout) == 0) {
            STMS_WARN("poll() timed out!");

            if (isUdp) {  DTLSv1_handle_timeout(ssl); }

            return false;
        }

        if (!(cliPollFd.revents & event)) {
            STMS_WARN("Desired flags not set in poll()!");
            if (isUdp) { DTLSv1_handle_timeout(ssl); }
            return false;
        }

        return true;
    }

    void _stms_SSLBase::setKeyPassword(const std::string &newPass) {
        password = newPass;
        SSL_CTX_set_default_passwd_cb_userdata(pCtx, &this->password);
        SSL_CTX_set_default_passwd_cb(pCtx, getPassword);
    }

    void _stms_SSLBase::setCertAuth(const std::string &caCert, const std::string &caPath) {
        if (!SSL_CTX_load_verify_locations(pCtx, caCert.empty() ? nullptr : caCert.c_str(),
                                           caPath.empty() ? nullptr : caPath.c_str())) {
            STMS_WARN("Failed to set the CA certs and paths for DTLS/TLS cli/server! Path='{}', cert='{}'", caPath, caCert);
            handleSSLError();
        }
    }

    void _stms_SSLBase::setPrivateKey(const std::string &key) {
        if (!SSL_CTX_use_PrivateKey_file(pCtx, key.c_str(), SSL_FILETYPE_PEM)) {
            STMS_WARN("Failed to set private key for DTLS/TLS cli/server (key='{}')", key);
            handleSSLError();
        }
    }

    void _stms_SSLBase::setPublicCert(const std::string &cert) {
        if (!SSL_CTX_use_certificate_chain_file(pCtx, cert.c_str())) {
            STMS_WARN("Failed to set public cert chain for DTLS/TLS Server (cert='{}')", cert);
            handleSSLError();
        }
    }

    bool _stms_SSLBase::verifyKeyMatchCert() {
        if (!SSL_CTX_check_private_key(pCtx)) {
            STMS_WARN("Public cert and private key mismatch in DTLS/TLS client/server!");
            handleSSLError();
            return false;
        }

        return true;
    }

    void _stms_SSLBase::moveSslBase(_stms_SSLBase *rhs) {
        movePlain(rhs);

        SSL_CTX_free(pCtx);

        pCtx = rhs->pCtx;
        password = std::move(rhs->password);

        rhs->pCtx = nullptr;
    }

    _stms_SSLBase &_stms_SSLBase::operator=(_stms_SSLBase &&rhs) noexcept {
        if (pCtx == rhs.pCtx || this == &rhs) {
            return *this;
        }

        moveSslBase(&rhs);

        return *this;
    }

    _stms_SSLBase::_stms_SSLBase(_stms_SSLBase &&rhs) noexcept {
        *this = std::move(rhs);
    }

    void _stms_SSLBase::trustDefault() {
        if (SSL_CTX_set_default_verify_paths(pCtx) != 1) {
            STMS_WARN("Failed to set default truststore!");
            handleSSLError();
        }
    }
}

