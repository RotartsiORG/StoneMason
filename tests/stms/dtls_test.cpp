//
// Created by grant on 4/22/20.
//

#include "gtest/gtest.h"
#include "stms/net/dtls.hpp"

namespace {
    TEST(DLTS, Server) {
        stms::net::DTLSServer serv = stms::net::DTLSServer("127.0.0.1");
        serv.start();
        serv.stop();
    }
}
