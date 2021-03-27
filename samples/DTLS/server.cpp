//
// Created by grant on 5/13/20.
//

#include "stms/net/ssl_server.hpp"
#include "stms/net/ssl_client.hpp"
#include "stms/stms.hpp"

volatile int semaphore = 0;

void readInp() {
    while (true) {
        std::string answer;
        std::cin >> answer;
        if (answer == "stop") {
            semaphore++;
        }
        STMS_INFO("A='{}'", answer);
    }
}

int main(int argc, char *argv[]) {
    std::thread inputReader{readInp};

    stms::initAll();
    stms::InstaPool pool{};

    stms::SSLServer serv = stms::SSLServer(&pool, argc > 2);
    serv.setTimeout(0);
    serv.setHostAddr("3000", "127.0.0.1");
    serv.setIPv6(false);
    serv.setCertAuth("./res/ssl/legit/ca-pub-cert.pem", "");
    serv.setPublicCert("./res/ssl/legit/serv-pub-cert.pem");
    serv.setPrivateKey("./res/ssl/legit/serv-priv-key.pem");
    serv.verifyKeyMatchCert();
    serv.setVerifyMode(0x00); // SSL_VERIFY_NONE
    
    serv.setRecvCallback([&](const stms::UUID &cliuuid, const sockaddr *const sockaddr, uint8_t *buf, int size) {
        STMS_INFO("RCV {} {} {} {}", cliuuid.buildStr(), stms::getAddrStr(sockaddr), reinterpret_cast<char*>(buf), size);
    });

    serv.setConnectCallback([](const stms::UUID &uuid, const sockaddr *const addr) {
        STMS_INFO("CN {} {}", uuid.buildStr(), stms::getAddrStr(addr));
    });

    serv.setDisconnectCallback([&](const stms::UUID &uuid, const sockaddr *const &addr) {
        STMS_INFO("DC {} {}", uuid.buildStr(), stms::getAddrStr(addr));
    });

    serv.start();

    while (serv.tick()) {
        if (semaphore > 0) {
            serv.stop();
        }

        serv.waitEvents(32);
        stms::flushSSLErrors();
    }

    serv.stop();
    stms::flushSSLErrors();

    inputReader.join();


}

// int main() {
//     stms::initAll();
//     stms::ThreadPool pool;
//     pool.start();

//     stms::SSLServer serv = stms::SSLServer(&pool, true);
//     serv.setTimeout(5000); // 5 sec timeout
//     serv.setHostAddr("3000", "127.0.0.1"); // can be left out but *shurgs*
//     serv.setIPv6(false);
//     serv.setCertAuth("./res/ssl/legit/ca-pub-cert.pem", "");
//     serv.setPublicCert("./res/ssl/legit/serv-pub-cert.pem");
//     serv.setPrivateKey("./res/ssl/legit/serv-priv-key.pem");
//     serv.verifyKeyMatchCert();

//     serv.setRecvCallback([&](const stms::UUID &cliuuid, const sockaddr *const sockaddr, uint8_t *buf, int size) {
//         buf[size] = '\0';
//         STMS_WARN("ECHOING '{}' ({} BYTES) TO CLIENT {} AT {}", reinterpret_cast<char *>(buf), size, cliuuid.buildStr(),
//                   stms::getAddrStr(sockaddr));
//         serv.send(cliuuid, buf, size);
//     });
//     serv.setConnectCallback([](const stms::UUID &uuid, const sockaddr *const addr) {
//         STMS_WARN("CONNECT: UUID={}, ADDR={}", uuid.buildStr(), stms::getAddrStr(addr));
//     });
//     serv.setDisconnectCallback([&](const stms::UUID &uuid, const sockaddr *const &addr) {
//         STMS_WARN("DISCONNECT: UUID={} ADDR={}", uuid.buildStr(), stms::getAddrStr(addr));
//     });

//     serv.start();
//     while (serv.tick()) {
//         serv.waitEvents(32); // 30 fps
//         stms::flushSSLErrors();
//     }

//     serv.stop();  // Stop is automatically called in destructor.
//     stms::flushSSLErrors();
// }