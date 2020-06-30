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

    void quitCurl();

    struct CurlResult {
        std::string data;

        double downloadNow;
        double downloadTotal;

        double uploadNow;
        double uploadTotal;

        bool success = false;
    };

    static size_t curlWriteCb(void *buf, size_t size, size_t nmemb, void *userDat);

    // https://curl.haxx.se/libcurl/c/libcurl-tutorial.html
    class CURLHandle {
    private:
        ThreadPool *pool{};

        std::mutex resultMtx;

        CURL *handle;
        std::string url;

        friend size_t curlWriteCb(void *buf, size_t size, size_t nmemb, void *userDat);
    public:
        std::string result;
//        std::string toWrite;

        double downloadSoFar = 0;
        double downloadTotal = 0;

        double uploadSoFar = 0;
        double uploadTotal = 0;

        explicit CURLHandle(ThreadPool *inPool);

        ~CURLHandle();

        CURLHandle() = delete;

        inline void setUrl(const char *cUrl) {
            curl_easy_setopt(handle, CURLOPT_URL, cUrl);
            url = std::string(cUrl);
        }

        inline std::string getUrl() {
            return url;
        }

        /**
         * Actually perform the operation that was configured
         * @return See libcurl documentation for `CURLcode`. You can treat is as a bool that is false if
         *         the operation was successful, true otherwise.
         */
        std::future<CURLcode> perform();


    };
}

#endif //__STONEMASON_CURL_HPP
