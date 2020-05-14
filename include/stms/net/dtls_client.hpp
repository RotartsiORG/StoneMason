//
// Created by grant on 5/4/20.
//

#ifndef __STONEMASON_DTLS_CLIENT_HPP
#define __STONEMASON_DTLS_CLIENT_HPP

#include <string>

#include "stms/async.hpp"
#include "stms/net/ssl.hpp"

#include "openssl/ssl.h"
#include "openssl/bio.h"
#include "openssl/err.h"
#include "openssl/rand.h"
#include "openssl/opensslv.h"

namespace stms::net {
    class DTLSClient : public SSLBase {
    private:
        BIO *pBio{};
        SSL *pSsl{};
        int timeouts{};

        void onStart() override;

        void onStop() override;
    public:
        DTLSClient() = default;

        explicit DTLSClient(stms::ThreadPool *pool, const std::string &addr = "any",
                            const std::string &port = "3000", bool preferV6 = true,
                            const std::string &certPem = "server-cert.pem",
                            const std::string &keyPem = "server-key.pem",
                            const std::string &caCert = "", const std::string &caPath = "",
                            const std::string &password = ""
        );

        ~DTLSClient() override;

        DTLSClient &operator=(const DTLSClient &rhs) = delete;

        DTLSClient(const DTLSClient &rhs) = delete;

        bool tick();

        DTLSClient &operator=(DTLSClient &&rhs);

        DTLSClient(DTLSClient &&rhs);
    };
}


#endif //__STONEMASON_DTLS_CLIENT_HPP
