//
// Created by grant on 4/22/20.
//

#include "gtest/gtest.h"
#include "stms/net/dtls.hpp"

namespace {
    TEST(DLTS, Server) {
        stms::ThreadPool pool;
        stms::net::DTLSServer serv = stms::net::DTLSServer(&pool, "Grant-PC.local", "3000", false,
                                                           "./res/ssl/serv-pub-cert.pem", "./res/ssl/serv-priv-key.pem",
                                                           "./res/ssl/ca-pub-cert.pem", "", "");
        serv.start();
        for (int i = 0; i < 100; i++) { serv.tick(); }
        serv.stop();  // Stop is automatically called in destructor.
        stms::net::flushSSLErrors();
    }
}
