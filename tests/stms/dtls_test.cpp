//
// Created by grant on 4/22/20.
//

#include "gtest/gtest.h"
#include "stms/net/dtls_server.hpp"

namespace {
    TEST(DLTS, Server) {
        stms::ThreadPool pool;
        pool.start();

        stms::net::DTLSServer serv = stms::net::DTLSServer(&pool, "Grant-PC.local", "3000", false,
                                                           "./res/ssl/serv-pub-cert.pem", "./res/ssl/serv-priv-key.pem",
                                                           "./res/ssl/ca-pub-cert.pem", "", "");

        serv.setRecvCallback([&](const std::string &cliuuid, const sockaddr *const sockaddr, uint8_t *buf, int size) {
            buf[size] = '\0';
            STMS_WARN("ECHOING '{}' ({} BYTES) TO CLIENT {} AT {}", reinterpret_cast<char *>(buf), size, cliuuid,
                      stms::net::getAddrStr(sockaddr));
            serv.send(cliuuid, buf, size);
        });
        serv.setConnectCallback([](const std::string &uuid, const sockaddr *const addr) {
            STMS_WARN("CONNECT: UUID={}, ADDR={}", uuid, stms::net::getAddrStr(addr));
        });
        serv.setDisconnectCallback([&](const std::string &uuid, const sockaddr *const addr) {
            STMS_WARN("DISCONNECT: UUID={} ADDR={}", uuid, stms::net::getAddrStr(addr));
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
