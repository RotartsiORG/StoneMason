//
// Created by grant on 5/4/20.
//

#include <stms/logging.hpp>
#include <poll.h>
#include "stms/net/ssl_client.hpp"

namespace stms {

    SSLClient::SSLClient(stms::ThreadPool *pool, bool udp) : _stms_SSLBase(false, pool, udp) {}

    SSLClient::~SSLClient() {
        if (running) {
            STMS_WARN("SSLClient destroyed whilst it was still running! Stopping it now...");
            stop();
        }
    }

    void SSLClient::onStart() {
        doShutdown = false;

        if (isUdp) {
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
        } else {
            pSsl = SSL_new(pCtx);
            SSL_set_fd(pSsl, sock);
        }

        STMS_INFO("DTLS/TLS client about to preform handshake!");
        int handshakeTimeouts = 0;
        while (handshakeTimeouts < maxTimeouts) {
            handshakeTimeouts++;
            try {
                int handshakeRet = handleSslGetErr(pSsl, SSL_connect(pSsl));
                if (handshakeRet == 1) {
                    STMS_INFO("Client handshake successful!");
                    handshakeTimeouts = 0;
                    break;
                }
            } catch (SSLWantReadException &) {
                STMS_INFO("WANT_READ returned from SSL_connect! Blocking until read-ready...");
                blockUntilReady(sock, pSsl, POLLIN);
            } catch (SSLWantWriteException &) {
                STMS_INFO("WANT_WRITE returned from SSL_connect! Blocking until write-ready...");
                blockUntilReady(sock, pSsl, POLLOUT);
            } catch (SSLFatalException &) {
                STMS_WARN("SSL_connect() error was fatal! Dropping connection!");
                stop();
                return;
            } catch (SSLException &) {
                STMS_INFO("Retrying SSL_connect()!");
            }
        }

        if (handshakeTimeouts >= maxTimeouts) {
            STMS_WARN("SSL_connect timed out completely ({}/{})! Dropping connection!", handshakeTimeouts, maxTimeouts);
            stop();
            return;
        }
        doShutdown = true;

        char certName[certAndCipherLen];
        char cipherName[certAndCipherLen];

        auto peerCert = SSL_get_certificate(pSsl);
        X509_NAME_oneline(X509_get_subject_name(peerCert), certName, certAndCipherLen);
        X509_free(peerCert);

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

    bool SSLClient::tick() {
        if (!running) {
            STMS_WARN("SSLClient::tick() called when stopped! Ignoring invocation!");
            return false;
        }

        if (SSL_get_shutdown(pSsl) & SSL_RECEIVED_SHUTDOWN) {
            stop();
            return false;
        }

        if (timeoutTimer.getTime() >= static_cast<float>(timeoutMs)) {
            if (isUdp) { DTLSv1_handle_timeout(pSsl); }
            STMS_INFO("Connection to server timed out! Dropping connection!");
            stop();
            return false;
        }

        if (recvfrom(sock, nullptr, 0, MSG_PEEK, nullptr, nullptr) == -1
             && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            return running;  // No data could be read
        }

        if (!isReading) {
            isReading = true;
            // lambda captures validated
            pPool->submitTask([&]() {
                int readTimeouts = 0;
                while (readTimeouts < maxTimeouts) {
                    readTimeouts++;

                    try {
                        uint8_t recvBuf[maxRecvLen];
                        int readLen = handleSslGetErr(pSsl, SSL_read(pSsl, recvBuf, maxRecvLen));

                        if (readLen > 0) {
                            readTimeouts = 0;
                            timeoutTimer.reset();
                            recvCallback(recvBuf, readLen);
                            break;
                        }
                    } catch (SSLWantReadException &) {
                        STMS_INFO("SSL_read() returned WANT_READ. Retrying!");
                        blockUntilReady(sock, pSsl, POLLIN);
                    } catch (SSLWantWriteException &) {
                        STMS_INFO("SSL_read() returned WANT_WRITE. Retrying!");
                        blockUntilReady(sock, pSsl, POLLOUT);
                    } catch (SSLFatalException &) {
                        STMS_WARN("Connection to server closed forcefully due to SSL_read() exception!");
                        doShutdown = false;

                        stop();
                        break;
                    } catch (SSLException &) {
                        STMS_INFO("SSL_read() failed! Retrying...");
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

    void SSLClient::onStop() {
        if (pSsl != nullptr && doShutdown) {
            int numTries = 0;
            while (numTries < sslShutdownMaxRetries) {
                numTries++;
                try {
                    int shutdownRet = handleSslGetErr(pSsl, SSL_shutdown(pSsl));
                    if (shutdownRet == 0) {
                        STMS_INFO("Waiting to hear back after SSL_shutdown... retrying");
                        continue;
                    } else if (shutdownRet == 1) {
                        break;
                    }
                } catch (SSLWantWriteException &) {
                    blockUntilReady(sock, pSsl, POLLOUT);
                } catch (SSLWantReadException &) {
                    blockUntilReady(sock, pSsl, POLLIN);
                } catch (SSLFatalException &) {
                    STMS_WARN("Skipping SSL_shutdown() because of fatal exception!");
                    break; // give up *tableflip*
                } catch (SSLException &) {
                    STMS_INFO("Retrying SSL_shutdown()!");
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

    std::future<int> SSLClient::send(const uint8_t *const msg, int msgLen, bool copy) {
        std::shared_ptr<std::promise<int>> prom = std::make_shared<std::promise<int>>();
        if (!running) {
            STMS_ERROR("SSLClient::send called when not connected! {} bytes dropped!", msgLen);
            prom->set_value(-1);
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

            int sendTimeouts = 0;
            while (sendTimeouts < maxTimeouts) {
                sendTimeouts++;

                try {
                    int ret = handleSslGetErr(pSsl, SSL_write(pSsl, capMsg, capLen));
                    if (ret > 0) {
                        sendTimeouts = 0;
                        timeoutTimer.reset();
                        capProm->set_value(ret);
                        break;
                    }
                } catch (SSLWantReadException &) {
                    STMS_WARN("send() failed with WANT_READ! Retrying!");
                    blockUntilReady(sock, pSsl, POLLIN);
                } catch (SSLWantWriteException &) {
                    STMS_WARN("send() failed with WANT_WRITE! Retrying!");
                    blockUntilReady(sock, pSsl, POLLOUT);
                } catch (SSLFatalException &) {
                    STMS_WARN("Connection to server at {} closed forcefully!", addrStr);
                    doShutdown = false;

                    stop();
                    capProm->set_value(-2);
                    break;
                } catch (SSLException &) {
                    STMS_INFO("Retrying SSL_write!");
                }
            }

            if (capCpy) {
                delete[] capMsg;
            }

            if (sendTimeouts >= maxTimeouts) {
                STMS_WARN("SSL_write() timed out completely! Dropping connection!");
                stop();
                capProm->set_value(-3);
            }
        });

        return prom->get_future();
    }

    void SSLClient::waitEvents(int pollTimeoutMs) {
        pollfd servPollFd{};
        servPollFd.events = POLLIN;
        servPollFd.fd = sock;

        poll(&servPollFd, 1, pollTimeoutMs);
    }

    SSLClient &SSLClient::operator=(SSLClient &&rhs) noexcept {
        if (pSsl == rhs.pSsl || pBio == rhs.pBio || pCtx == rhs.pCtx || this == &rhs) {
            return *this;
        }

        if (rhs.isRunning()) {
            STMS_WARN("SSLClient moved while running! Stopping it now! We will disconnect from the server.");
            stop();
        }

        if (running) { stop(); }

        pBio = rhs.pBio;
        pSsl = rhs.pSsl;
        doShutdown = rhs.doShutdown;
        isReading = rhs.isReading;
        timeoutTimer = rhs.timeoutTimer;
        recvCallback = rhs.recvCallback;
        internalOpEq(&rhs);

        rhs.pSsl = nullptr;
        rhs.pBio = nullptr;

        return *this;
    }

    SSLClient::SSLClient(SSLClient &&rhs) noexcept {
        *this = std::move(rhs);
    }
}
