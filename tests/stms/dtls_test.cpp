//
// Created by grant on 4/22/20.
//

#include "gtest/gtest.h"
#include "stms/net/dtls_server.hpp"

namespace {
    TEST(DLTS, Server) {
        stms::ThreadPool pool;
        stms::net::DTLSServer serv = stms::net::DTLSServer(&pool, "Grant-PC.local", "3000", false,
                                                           "./res/ssl/serv-pub-cert.pem", "./res/ssl/serv-priv-key.pem",
                                                           "./res/ssl/ca-pub-cert.pem", "", "");

        serv.setRecvCallback([&](const std::string &cliuuid, const sockaddr *const sockaddr, uint8_t *buf, int size) {
            STMS_FATAL("ECHOING {} BYTES", size);
            serv.send(cliuuid, buf, size);
        });
        serv.setConnectCallback([](const std::string &uuid, const sockaddr *const addr) {
            STMS_FATAL("CONNECT!");
        });
        serv.setDisconnectCallback([&](const std::string &uuid, const sockaddr *const addr) {
            STMS_FATAL("DISCONNECT!");
            serv.stop();
        });

        serv.start();
        while (serv.tick()) {
            stms::net::flushSSLErrors();
        }

        serv.stop();  // Stop is automatically called in destructor.
        stms::net::flushSSLErrors();
    }
}
