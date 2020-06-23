//
// Created by grant on 5/4/20.
//

#ifndef __STONEMASON_NET_DTLS_CLIENT_HPP
#define __STONEMASON_NET_DTLS_CLIENT_HPP

#include <string>
#include <stms/timers.hpp>

#include "stms/async.hpp"
#include "stms/net/ssl.hpp"

#include "openssl/ssl.h"
#include "openssl/bio.h"
#include "openssl/err.h"
#include "openssl/rand.h"
#include "openssl/opensslv.h"

namespace stms {
    class DTLSClient : public _stms_SSLBase {
    private:
        BIO *pBio{};
        SSL *pSsl{};
        int timeouts{};
        bool doShutdown = false;
        bool isReading = false;
        Stopwatch timeoutTimer{};

        void onStart() override;

        void onStop() override;

        std::function<void(uint8_t *, size_t)> recvCallback = [](uint8_t *, size_t) {};
        // Connect/Disconnect callback?
    public:
        DTLSClient() = default;

        explicit DTLSClient(stms::ThreadPool *pool, const std::string &addr = "any",
                            const std::string &port = "3000", bool preferV6 = true,
                            const std::string &certPem = "server-cert.pem",
                            const std::string &keyPem = "server-key.pem",
                            const std::string &caCert = "", const std::string &caPath = "",
                            const std::string &password = ""
        );

        inline void setRecvCallback(const std::function<void(uint8_t *, size_t)> &newCb) {
            recvCallback = newCb;
        }

        std::future<int> send(const uint8_t *const, int);

        ~DTLSClient() override;

        DTLSClient &operator=(const DTLSClient &rhs) = delete;

        DTLSClient(const DTLSClient &rhs) = delete;

        bool tick();

        size_t getMtu();

        DTLSClient &operator=(DTLSClient &&rhs);

        DTLSClient(DTLSClient &&rhs);
    };
}


#endif //__STONEMASON_NET_DTLS_CLIENT_HPP
