//
// Created by grant on 8/15/20.
//

#include "gtest/gtest.h"
#include "stms/net/ssl_client.hpp"
#include "stms/net/ssl_server.hpp"
#include "stms/net/plain_udp.hpp"


namespace {
    class SSLTest : public ::testing::Test {
    protected:
        stms::ThreadPool *pool{};
        stms::SSLServer *serv{};
        stms::SSLClient *cli{};

        bool serverPinged = false;
        bool cliPinged = false;

        void SetUp() override {
            serverPinged = false;
            cliPinged = false;

            pool = new stms::ThreadPool();
            pool->start();
        }

        void TearDown() override {
            cli->stop();
            serv->stop();

            pool->waitIdle(0);
            pool->stop(true);

            delete cli;
            delete serv;
            delete pool;
        }

        void start(bool isUdp, bool dubiousCertsCli, bool dubiousCertsServ) {
            serv = new stms::SSLServer(pool, isUdp);
            serv->setHostAddr("3000", "127.0.0.1");
            serv->setIPv6(false);
            serv->setCertAuth("./res/ssl/legit/ca-pub-cert.pem", "");
            if (dubiousCertsServ) {
                serv->setPublicCert("./res/ssl/dubious/serv-pub-cert.pem");
                serv->setPrivateKey("./res/ssl/dubious/serv-priv-key.pem");
            } else {
                serv->setPublicCert("./res/ssl/legit/serv-pub-cert.pem");
                serv->setPrivateKey("./res/ssl/legit/serv-priv-key.pem");
            }
            serv->verifyKeyMatchCert();
            serv->setConnectCallback([&](const stms::UUID &c, const sockaddr *const addr) {
                STMS_WARN("CONNECT {}: {}", c.buildStr(), stms::getAddrStr(addr));
            });

            serv->setDisconnectCallback([](const stms::UUID &c, const sockaddr *const addr) {
                STMS_WARN("DISCONNECT {}: {}", c.buildStr(), stms::getAddrStr(addr));
            });
            serv->setRecvCallback([&](const stms::UUID &c, const sockaddr *const addr, uint8_t *dat, int size) {
                EXPECT_EQ(size, 5);
                dat[size] = '\0';
                std::string str = std::string(reinterpret_cast<char *>(dat));

                STMS_WARN("RECV {} from {}: {} BYTES: '{}'", c.buildStr(), stms::getAddrStr(addr), size, str);
                auto fut = serv->send(c, dat, size, true);
                EXPECT_EQ(fut.get(), 5);
                EXPECT_EQ(str, "HELLO");
                std::this_thread::sleep_for(std::chrono::milliseconds(125));
                serv->stop();
            });


            cli = new stms::SSLClient(pool, isUdp);
            cli->setHostAddr("3000", "127.0.0.1");
            cli->setIPv6(false);
            cli->setCertAuth("./res/ssl/legit/ca-pub-cert.pem", "");
            if (dubiousCertsCli) {
                cli->setPublicCert("./res/ssl/dubious/cli-pub-cert.pem");
                cli->setPrivateKey("./res/ssl/dubious/cli-priv-key.pem");
            } else {
                cli->setPublicCert("./res/ssl/legit/cli-pub-cert.pem");
                cli->setPrivateKey("./res/ssl/legit/cli-priv-key.pem");
            }
            cli->verifyKeyMatchCert();

            cli->setRecvCallback([&](uint8_t *dat, size_t size) {
                EXPECT_EQ(size, 5);
                dat[size] = '\0';
                std::string str = std::string(reinterpret_cast<char *>(dat));
                STMS_WARN("CLI RECV {} BYTES: {}", size, str);
                EXPECT_EQ(str, "HELLO");

                cli->stop();
            });

            serv->start();
            pool->submitTask([&]() {
                std::this_thread::sleep_for(std::chrono::milliseconds(125));
                cli->start();

                if (dubiousCertsServ || dubiousCertsCli) {
                    EXPECT_FALSE(cli->isRunning());
                    cli->stop();
                    serv->stop();
                    return;
                } else {
                    EXPECT_TRUE(cli->isRunning());
                }

                const char *msg = "HELLO";
                auto fut = cli->send(reinterpret_cast<const uint8_t *>(msg), 5, true);
                EXPECT_EQ(fut.get(), 5);
//                STMS_WARN("CLI SEND RETURNED {}", fut.get());

                while (cli->tick()) {
                    cli->waitEvents(16);
                }
            });

            while (serv->tick()) {
                serv->waitEvents(16);
            }
        }
    };

    TEST_F(SSLTest, TCP) {
        start(false, false, false);
    }

    TEST_F(SSLTest, UDP) {
        start(true, false, false);
    }

    TEST_F(SSLTest, DubiousServer) {
        start(false, false, true);
    }

    TEST_F(SSLTest, DubiousCli) {
        start(true, true, false);
    }

    TEST(PlainTest, UDP) {
        stms::ThreadPool p{};

        stms::UDPPeer serv{true, &p};
        stms::UDPPeer cli{false, &p};

        cli.setRecvCallback([&](const sockaddr *const addr, socklen_t alen, uint8_t *buf, ssize_t len) {
            EXPECT_EQ(len, 4);
            buf[len] = '\0';

            std::string str = std::string(reinterpret_cast<char *>(buf));
            STMS_WARN("CLI RECV FROM {} (l={}) {} BYTES: {}", stms::getAddrStr(addr), alen, len, str);
            EXPECT_EQ(str, "PING");

            serv.stop();
            cli.stop();
        });

        serv.setRecvCallback([&](const sockaddr *const addr, socklen_t alen, uint8_t *buf, ssize_t len) {
            EXPECT_EQ(len, 6);
            buf[len] = '\0';

            std::string str = std::string(reinterpret_cast<char *>(buf));
            STMS_WARN("SERV RECV FROM {} (l={}) {} BYTES: {}", stms::getAddrStr(addr), alen, len, str);
            EXPECT_EQ(str, "CLIENT");

            char *ping = "PING";
            serv.sendTo(addr, alen, reinterpret_cast<uint8_t *>(ping), 4, true);
        });

        serv.setIPv6(false); cli.setIPv6(false); 
        serv.setHostAddr("3000", "127.0.0.1"); cli.setHostAddr("3000", "127.0.0.1");

        p.start();
        serv.start();

        p.submitTask([&]() {
            cli.start();

            char *str = "CLIENT";
            cli.send(reinterpret_cast<uint8_t *>(str), 6, true);

            while (cli.tick()) {
                cli.waitEvents(16);
            }
        });

        while (serv.tick()) {
            serv.waitEvents(16);
        }
    }
}

