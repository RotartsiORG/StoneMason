//
// Created by grant on 5/13/20.
//

#include "stms/net/ssl_server.hpp"
#include "stms/net/ssl_client.hpp"
#include "stms/stms.hpp"

int main() {
    stms::initAll();
    stms::InstaPool pool;

    stms::SSLClient cli = stms::SSLClient(&pool, true);
    cli.setTimeout(0);
    cli.setHostAddr("3000", "127.0.0.1"); // can be omitted.
    cli.setIPv6(false);
    cli.setCertAuth("./res/ssl/legit/ca-pub-cert.pem", "");
    cli.setPublicCert("./res/ssl/legit/cli-pub-cert.pem");
    cli.setPrivateKey("./res/ssl/legit/cli-priv-key.pem");
    cli.verifyKeyMatchCert();
    cli.setVerifyMode(0);

    cli.setRecvCallback([](uint8_t *in, int size) {
        in[size] = '\0';
        STMS_WARN("Client recv {}", reinterpret_cast<char *>(in));
    });


    cli.start();
    cli.send(reinterpret_cast<const uint8_t *>("Hello world!"), 12, true);
    while (cli.tick()) {
        cli.waitEvents(32); // 30 fps
        stms::flushSSLErrors();
    }

    if (cli.isRunning()) {
        cli.stop();  // Stop is automatically called in destructor.
    }
    stms::flushSSLErrors();
    pool.waitIdle(1000);
}


// int main() {
//     stms::initAll();
//     stms::ThreadPool pool;
//     pool.start();

//     stms::SSLClient cli = stms::SSLClient(&pool, true);
//     cli.setTimeout(5000); // 5 sec timeout
//     cli.setHostAddr("3000", "127.0.0.1"); // can be omitted.
//     cli.setIPv6(false);
//     cli.setCertAuth("./res/ssl/legit/ca-pub-cert.pem", "");
//     cli.setPublicCert("./res/ssl/legit/cli-pub-cert.pem");
//     cli.setPrivateKey("./res/ssl/legit/cli-priv-key.pem");
//     cli.verifyKeyMatchCert();

//     cli.setRecvCallback([](uint8_t *in, int size) {
//         in[size] = '\0';
//         STMS_WARN("Client recv {}", reinterpret_cast<char *>(in));
//     });


//     cli.start();
//     cli.send(reinterpret_cast<const uint8_t *>("Hello world!"), 12, true);
//     while (cli.tick()) {
//         cli.waitEvents(32); // 30 fps
//         stms::flushSSLErrors();
//     }

//     if (cli.isRunning()) {
//         cli.stop();  // Stop is automatically called in destructor.
//     }
//     stms::flushSSLErrors();
//     pool.waitIdle(1000);
// }
