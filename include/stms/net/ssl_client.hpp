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
    class SSLClient : public _stms_SSLBase {
    private:
        BIO *pBio{};
        SSL *pSsl{};
        bool doShutdown = false;
        bool isReading = false;
        Stopwatch timeoutTimer{};

        void onStart() override;

        void onStop() override;

        std::function<void(uint8_t *, size_t)> recvCallback = [](uint8_t *, size_t) {};
        // Connect/Disconnect callback?
    public:

        explicit SSLClient(stms::ThreadPool *pool, bool isUdp);

        inline void setRecvCallback(const std::function<void(uint8_t *, size_t)> &newCb) {
            recvCallback = newCb;
        }

        std::future<int> send(const uint8_t *const, int, bool copy = false);

        void waitEvents(int timeoutMs) override;

        ~SSLClient() override;

        SSLClient &operator=(const SSLClient &rhs) = delete;

        SSLClient(const SSLClient &rhs) = delete;

        bool tick();

        inline size_t getMtu() {
            return DTLS_get_data_mtu(pSsl);
        };

        SSLClient &operator=(SSLClient &&rhs);

        SSLClient(SSLClient &&rhs);
    };
}


#endif //__STONEMASON_NET_DTLS_CLIENT_HPP
