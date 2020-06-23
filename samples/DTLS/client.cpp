//
// Created by grant on 5/13/20.
//

#include "stms/net/dtls_server.hpp"
#include "stms/net/dtls_client.hpp"

int main() {
    stms::initAll();
    stms::ThreadPool pool;
    pool.start();

    stms::DTLSClient cli = stms::DTLSClient(&pool, "Grant-PC.local", "3000", false,
                                                      "./res/ssl/cli-pub-cert.pem", "./res/ssl/cli-priv-key.pem",
                                                      "./res/ssl/ca-pub-cert.pem", "", "");
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
