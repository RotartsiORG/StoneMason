//
// Created by grant on 4/30/20.
//

#pragma once

#ifndef __STONEMASON_SSL_HPP
#define __STONEMASON_SSL_HPP

#include "openssl/ssl.h"
#include <string>
#include "sys/socket.h"

namespace stms::net {
    void initOpenSsl();

    int handleSslGetErr(SSL *, int);

    unsigned long handleSSLError();

    inline void flushSSLErrors() {
        while (handleSSLError() != 0);
    }

    std::string getAddrStr(const sockaddr *const addr);
}

#endif //__STONEMASON_SSL_HPP
