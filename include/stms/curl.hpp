//
// Created by grant on 6/2/20.
//

#pragma once

#ifndef __STONEMASON_CURL_HPP
#define __STONEMASON_CURL_HPP

#include "curl/curl.h"

#include "stms/async.hpp"

#include <string>

namespace stms {
    void initCurl();

    struct CurlResult {
        std::string data;

        double downloadNow;
        double downloadTotal;

        double uploadNow;
        double uploadTotal;

        bool success = false;
    };

    // TODO: Username and passcode stuff, cert authentication
    std::future<CurlResult> readURL(const char *url, ThreadPool *pool, unsigned priority = 8);

    // TODO: writeURL()
}

#endif //__STONEMASON_CURL_HPP
