//
// Created by grant on 5/4/20.
//

#include <stms/logging.hpp>
#include <poll.h>
#include "stms/net/dtls_client.hpp"

namespace stms::net {

    DTLSClient::DTLSClient(stms::ThreadPool *pool, const std::string &addr, const std::string &port, bool preferV6,
                           const std::string &certPem, const std::string &keyPem, const std::string &caCert,
                           const std::string &caPath, const std::string &password) : SSLBase(false, pool, addr,
                                                                                             port, preferV6, certPem,
                                                                                             keyPem, caCert, caPath,
                                                                                             password) {

    }

    DTLSClient::~DTLSClient() {
        stop();

        if (pSsl != nullptr) {
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
    }

    void DTLSClient::onStart() {
        pBio = BIO_new_dgram(sock, BIO_NOCLOSE);
        BIO_ctrl(pBio, BIO_CTRL_DGRAM_SET_CONNECTED, 0, &pAddr->ai_addr);
        pSsl = SSL_new(pCtx);
        SSL_set_bio(pSsl, pBio, pBio);

        STMS_INFO("DTLS client about to preform handshake!");
        bool doHandshake = true;
        int handshakeTimeouts = 0;
        while (doHandshake && handshakeTimeouts < maxTimeouts) {
            int handshakeRet = SSL_connect(pSsl);

            if (handshakeRet == 1) {
                doHandshake = false;
            } else if (handshakeRet == 0) {
                STMS_INFO("DTLS client handshake failed! Cannot connect!");
                handleSslGetErr(pSsl, handshakeRet);
                isRunning = false;
                return;
            } else if (handshakeRet < 0) {
                STMS_WARN("DTLS client handshake error!");
                handshakeRet = handleSslGetErr(pSsl, handshakeRet);
                if (handshakeRet == -5 || handshakeRet == -1 || handshakeRet == -999 ||
                        handshakeRet == -6) {
                    STMS_WARN("SSL_connect() error was fatal! Dropping connection!");
                    isRunning = false;
                    return;
                } else if (handshakeRet == -2) { // Read
                    STMS_INFO("WANT_READ returned from SSL_connect! Blocking until read-ready...");
                    if (!stms::net::SSLBase::blockUntilReady(sock, pSsl, POLLIN)) {
                        STMS_WARN("SSL_connect() WANT_READ timed out!");
                        handshakeTimeouts++;
                        continue;
                    }
                } else if (handshakeRet == -3) { // Write
                    STMS_INFO("WANT_WRITE returned from SSL_connect! Blocking until write-ready...");
                    if (!stms::net::SSLBase::blockUntilReady(sock, pSsl, POLLOUT)) {
                        STMS_WARN("SSL_connect() WANT_WRITE timed out!");
                        handshakeTimeouts++;
                        continue;
                    }
                }

                STMS_INFO("Retrying SSL_connect()!");
            }
        }

        if (handshakeTimeouts >= maxTimeouts) {
            STMS_WARN("SSL_connect timed out completely! Dropping connection!");
            isRunning = false;
            return;
        }

        timeval timeout{};
        timeout.tv_sec = timeoutMs / 1000;
        timeout.tv_usec = (timeoutMs % 1000) * 1000;
        BIO_ctrl(pBio, BIO_CTRL_DGRAM_SET_RECV_TIMEOUT, 0, &timeout);

        char certName[certAndCipherLen];
        char cipherName[certAndCipherLen];
        X509_NAME_oneline(X509_get_subject_name(SSL_get_certificate(pSsl)), certName, certAndCipherLen);
        SSL_CIPHER_description(SSL_get_current_cipher(pSsl), cipherName, certAndCipherLen);

        const char *compression = SSL_COMP_get_name(SSL_get_current_compression(pSsl));
        const char *expansion = SSL_COMP_get_name(SSL_get_current_expansion(pSsl));

        STMS_INFO("Connected to server at {}: {}", addrStr, SSL_state_string_long(pSsl));
        STMS_INFO("Connected with cert: {}", certName);
        STMS_INFO("Connected using cipher {}",  cipherName);
        STMS_INFO("Connected using compression {} and expansion {}",
                  compression == nullptr ? "NULL" : compression,
                  expansion == nullptr ? "NULL" : expansion);
    }
}
