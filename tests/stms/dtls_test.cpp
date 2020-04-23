//
// Created by grant on 4/22/20.
//

#include "gtest/gtest.h"
#include "stms/net/dtls.hpp"

namespace {
    TEST(DLTS, Server) {
        stms::net::DTLSServer serv = stms::net::DTLSServer("192.168.1.83");
        serv.start();
        serv.stop();
    }
}
