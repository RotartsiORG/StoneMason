//
// Created by grant on 5/4/20.
//

#include <stms/logging.hpp>
#include <poll.h>
#include "stms/net/dtls_client.hpp"

namespace stms {

    DTLSClient::DTLSClient(stms::ThreadPool *pool) : _stms_SSLBase(false, pool) {}

    DTLSClient::~DTLSClient() {
        if (running) {
            STMS_PUSH_WARNING("DTLSClient destroyed whilst it was still running! Stopping it now...");
            stop();
        }
    }

    void DTLSClient::onStart() {
        doShutdown = false;
        pBio = BIO_new_dgram(sock, BIO_NOCLOSE);
        BIO_ctrl(pBio, BIO_CTRL_DGRAM_SET_CONNECTED, 0, &pAddr->ai_addr);

        timeval timeout{};
        timeout.tv_sec = timeoutMs / 1000;
        timeout.tv_usec = (timeoutMs % 1000) * 1000;
        BIO_ctrl(pBio, BIO_CTRL_DGRAM_SET_RECV_TIMEOUT, 0, &timeout);
        BIO_ctrl(pBio, BIO_CTRL_DGRAM_SET_SEND_TIMEOUT, 0, &timeout);

        BIO_ctrl(pBio, BIO_CTRL_DGRAM_MTU_DISCOVER, 0, nullptr);

        pSsl = SSL_new(pCtx);
        SSL_set_bio(pSsl, pBio, pBio);

        STMS_INFO("DTLS client about to preform handshake!");
        bool doHandshake = true;
        int handshakeTimeouts = 0;
        while (doHandshake && handshakeTimeouts < maxTimeouts) {
            int handshakeRet = SSL_connect(pSsl);
            handshakeTimeouts++;

            if (handshakeRet == 1) {
                doHandshake = false;
                handshakeTimeouts = 0;
                STMS_INFO("Client handshake successful!");
            } else if (handshakeRet == 0) {
                STMS_INFO("DTLS client handshake failed! Cannot connect!");
                handleSslGetErr(pSsl, handshakeRet);
                running = false;
                return;
            } else if (handshakeRet < 0) {
                STMS_WARN("DTLS client handshake error!");
                handshakeRet = handleSslGetErr(pSsl, handshakeRet);
                if (handshakeRet == -5 || handshakeRet == -1 || handshakeRet == -999 ||
                        handshakeRet == -6) {
                    STMS_WARN("SSL_connect() error was fatal! Dropping connection!");
                    running = false;
                    return;
                } else if (handshakeRet == -2) { // Read
                    STMS_INFO("WANT_READ returned from SSL_connect! Blocking until read-ready...");
                    if (!stms::_stms_SSLBase::blockUntilReady(sock, pSsl, POLLIN)) {
                        STMS_WARN("SSL_connect() WANT_READ timed out!");
                        continue;
                    }
                } else if (handshakeRet == -3) { // Write
                    STMS_INFO("WANT_WRITE returned from SSL_connect! Blocking until write-ready...");
                    if (!stms::_stms_SSLBase::blockUntilReady(sock, pSsl, POLLOUT)) {
                        STMS_WARN("SSL_connect() WANT_WRITE timed out!");
                        continue;
                    }
                }

                STMS_INFO("Retrying SSL_connect()!");
            }
        }

        if (handshakeTimeouts >= maxTimeouts) {
            STMS_WARN("SSL_connect timed out completely ({}/{})! Dropping connection!", handshakeTimeouts, maxTimeouts);
            running = false;
            return;
        }
        doShutdown = true;

        char certName[certAndCipherLen];
        char cipherName[certAndCipherLen];
        X509_NAME_oneline(X509_get_subject_name(SSL_get_certificate(pSsl)), certName, certAndCipherLen);
        SSL_CIPHER_description(SSL_get_current_cipher(pSsl), cipherName, certAndCipherLen);

        const char *compression = SSL_COMP_get_name(SSL_get_current_compression(pSsl));
        const char *expansion = SSL_COMP_get_name(SSL_get_current_expansion(pSsl));

        STMS_INFO("Connected to server at {}: {}", addrStr, SSL_state_string_long(pSsl));
        STMS_INFO("Connected with cert: {}", certName);
        STMS_INFO("Connected using cipher {}", cipherName);
        STMS_INFO("Connected using compression {} and expansion {}",
                  compression == nullptr ? "NULL" : compression,
                  expansion == nullptr ? "NULL" : expansion);
        timeoutTimer.start();
    }

    bool DTLSClient::tick() {
        if (!running) {
            STMS_PUSH_WARNING("DTLSClient::tick() called when stopped! Ignoring invocation!");
            return false;
        }

        if (SSL_get_shutdown(pSsl) & SSL_RECEIVED_SHUTDOWN || timeouts >= maxConnectionTimeouts) {
            stop();
            return false;
        }

        if (timeoutTimer.getTime() >= timeoutMs) {
            timeouts++;
            timeoutTimer.reset();
            DTLSv1_handle_timeout(pSsl);
            STMS_INFO("Connection to server timed out! (timeout #{})", timeouts);
        }

        if (recvfrom(sock, nullptr, 0, MSG_PEEK, pAddr->ai_addr,
                     &pAddr->ai_addrlen) == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            return running;  // No data could be read
        }

        if (!isReading) {
            isReading = true;
            // lambda captures validated
            pPool->submitTask([&]() {

                bool retryRead = true;
                int readTimeouts = 0;
                while (retryRead && timeouts < 1 && readTimeouts < maxTimeouts) {
                    readTimeouts++;

                    if (!stms::_stms_SSLBase::blockUntilReady(sock, pSsl, POLLIN)) {
                        STMS_WARN("SSL_read() timed out!");
                        continue;
                    }

                    uint8_t recvBuf[maxRecvLen];
                    int readLen = SSL_read(pSsl, recvBuf, maxRecvLen);
                    readLen = handleSslGetErr(pSsl, readLen);

                    if (readLen > 0) {
                        timeoutTimer.reset();
                        recvCallback(recvBuf, readLen);
                        retryRead = false;
                    } else if (readLen == -2) {
                        STMS_INFO("SSL_read() returned WANT_READ. Will retry next loop.");
                        retryRead = false;
                    } else if (readLen == -1 || readLen == -5 || readLen == -6) {
                        STMS_WARN("Connection to server closed forcefully!");
                        doShutdown = false;

                        stop();
                        retryRead = false;
                    } else if (readLen == -999) {
                        STMS_WARN("Disconnected from server for: Unknown error");

                        stop();
                        retryRead = false;
                    } else {
                        STMS_WARN("Server SSL_read failed for the reason above! Retrying!");
                        // Retry.
                    }
                }

                if (readTimeouts >= maxTimeouts) {
                    STMS_WARN("SSL_read() timed out completely! Dropping connection!");
                    stop();
                }
                isReading = false;
            });
        }

        return running;
    }

    void DTLSClient::onStop() {
        if (pSsl != nullptr && doShutdown) {
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
                        if (!stms::_stms_SSLBase::blockUntilReady(sock, pSsl, POLLIN)) {
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
        doShutdown = false;

        SSL_free(pSsl);
        pSsl = nullptr; // Don't double-free!
    }

    std::future<int> DTLSClient::send(const uint8_t *const msg, int msgLen, bool copy) {
        std::shared_ptr<std::promise<int>> prom = std::make_shared<std::promise<int>>();
        if (!running) {
            STMS_PUSH_ERROR("DTLSClient::send called when not connected! {} bytes dropped!", msgLen);
            prom->set_value(-114);
            return prom->get_future();
        }

        uint8_t *passIn{};
        if (copy) {
            passIn = new uint8_t[msgLen];
            std::copy(msg, msg + msgLen, passIn);
        } else {
            // I am forced to do this as std::copy would be impossible otherwise. We don't actually do anything with
            // the non-const-ness though.
            passIn = const_cast<uint8_t *>(msg);
        }

        // lambda captures validated
        pPool->submitTask([&, capProm{prom}, capMsg{passIn}, capLen{msgLen}, capCpy{copy}]() {
            int ret = -3;

            int sendTimeouts = 0;
            while (ret == -3 && sendTimeouts < maxTimeouts) {
                sendTimeouts++;
                if (!stms::_stms_SSLBase::blockUntilReady(sock, pSsl, POLLOUT)) {
                    STMS_WARN("SSL_write() timed out!");
                    continue;
                }

                ret = SSL_write(pSsl, capMsg, capLen);
                ret = handleSslGetErr(pSsl, ret);

                if (ret > 0) {
                    capProm->set_value(ret);
                    return;
                }

                if (ret == -3) {
                    STMS_WARN("send() failed with WANT_WRITE! Retrying!");
                } else if (ret == -1 || ret == -5 || ret == -6) {
                    STMS_WARN("Connection to server at {} closed forcefully!", addrStr);
                    doShutdown = false;

                    stop();
                    capProm->set_value(ret);
                    return;
                } else if (ret == -999) {
                    STMS_WARN("Disconnecting from server at {} for: Unknown error", addrStr);

                    stop();
                    capProm->set_value(ret);
                    return;
                } else if (ret < 1) {
                    STMS_WARN("SSL_write failed for the reason above! Retrying!");
                }
                STMS_INFO("Retrying SSL_write!");
            }

            if (capCpy) {
                delete[] capMsg;
            }

            if (sendTimeouts >= maxTimeouts) {
                STMS_WARN("SSL_write() timed out completely! Dropping connection!");
                stop();
            }

            capProm->set_value(ret);
        });

        return prom->get_future();
    }

    void DTLSClient::waitEvents(int pollTimeoutMs) {
        pollfd servPollFd{};
        servPollFd.events = POLLIN;
        servPollFd.fd = sock;

        poll(&servPollFd, 1, pollTimeoutMs);
    }
}
