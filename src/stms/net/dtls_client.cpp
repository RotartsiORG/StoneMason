//
// Created by grant on 5/4/20.
//

#include <stms/logging.hpp>
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
        handleSslGetErr(pSsl, SSL_connect(pSsl));
    }
}
