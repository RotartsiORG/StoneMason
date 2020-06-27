//
// Created by grant on 5/13/20.
//

#include "stms/net/dtls_server.hpp"
#include "stms/net/dtls_client.hpp"

int main() {
    stms::initAll();
    stms::ThreadPool pool;
    pool.start();

    stms::DTLSClient cli = stms::DTLSClient(&pool);
    cli.setHostAddr(); // can be omitted.
    cli.setIPv6(false);
    cli.setCertAuth("./res/ssl/ca-pub-cert.pem", "");
    cli.setPublicCert("./res/ssl/cli-pub-cert.pem");
    cli.setPrivateKey("./res/ssl/cli-priv-key.pem");
    cli.verifyKeyMatchCert();

    cli.setRecvCallback([](uint8_t *in, int size) {

    });


    cli.start();
    while (cli.tick());
    if (cli.isRunning()) {
        cli.stop();  // Stop is automatically called in destructor.
    }
    stms::flushSSLErrors();
    pool.waitIdle();
}
