//
// Created by grant on 5/13/20.
//

#include "stms/net/dtls_server.hpp"
#include "stms/net/dtls_client.hpp"

int main() {
    stms::initAll();
    stms::ThreadPool pool;
    pool.start();

    stms::DTLSServer serv = stms::DTLSServer(&pool, "Grant-PC.local", "3000", false,
                                                       "./res/ssl/serv-pub-cert.pem", "./res/ssl/serv-priv-key.pem",
                                                       "./res/ssl/ca-pub-cert.pem", "", "");

    serv.setRecvCallback([&](const std::string &cliuuid, const sockaddr *const sockaddr, uint8_t *buf, int size) {
        buf[size] = '\0';
        STMS_WARN("ECHOING '{}' ({} BYTES) TO CLIENT {} AT {}", reinterpret_cast<char *>(buf), size, cliuuid,
                  stms::getAddrStr(sockaddr));
        serv.send(cliuuid, buf, size);
    });
    serv.setConnectCallback([](const std::string &uuid, const sockaddr *const addr) {
        STMS_WARN("CONNECT: UUID={}, ADDR={}", uuid, stms::getAddrStr(addr));
    });
    serv.setDisconnectCallback([&](const std::string &uuid, const std::string &addr) {
        STMS_WARN("DISCONNECT: UUID={} ADDR={}", uuid, addr);
    });

    serv.start();
    while (serv.tick()) {
        stms::flushSSLErrors();
    }

    serv.stop();  // Stop is automatically called in destructor.
    stms::flushSSLErrors();
}