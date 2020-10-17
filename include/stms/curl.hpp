/**
 * @file stms/curl.hpp
 * @brief C++ wrapper around libCurl (https://curl.haxx.se/libcurl/)
 * Created by grant on 6/2/20.
 */

#pragma once

#ifndef __STONEMASON_CURL_HPP
#define __STONEMASON_CURL_HPP
//!< Include guard

#include "curl/curl.h"

#include "stms/async.hpp"

#include <string>

namespace stms {
    void initCurl(); //!< Init the curl module. Use `stms::initAll` if you wish to init everything.

    void quitCurl(); //!< Quit the curl module. No need to call this if you used `stms::initAll`.

    static size_t curlWriteCb(void *buf, size_t size, size_t nmemb, void *userDat); //!< Internal impl detail. Don't touch

    // https://curl.haxx.se/libcurl/c/libcurl-tutorial.html
    /**
     * @brief An object representing a connection/operation. See libCurl docs for `curl_easy_init()`
     */
    class CURLHandle {
    private:
        std::atomic<ThreadPool *> pool{}; //!< Thread pool to submit async tasks to

        std::mutex resultMtx; //!< Mutex for syncing writes to the result string

        std::atomic<CURL *> handle; //!< Internal impl detail. Don't touch
        std::string url; //!< Target URL to connect to

        friend size_t curlWriteCb(void *buf, size_t size, size_t nmemb, void *userDat); //!< internal impl detail.
    public:
        std::string result; //!< String of the contents we received back from the URL

        // in the future we may support uploading stuff
        // std::string toWrite;

        double downloadSoFar = 0; //!< Number of bytes we have downloaded so far
        double downloadTotal = 0; //!< Total number of bytes we have to download

        double uploadSoFar = 0; //!< Number of bytes we have uploaded so far
        double uploadTotal = 0; //!< Total number of bytes we have to upload

        /**
         * @brief Constructor.
         * @param inPool Pool we want to submit tasks to
         */
        explicit CURLHandle(ThreadPool *inPool);

        virtual ~CURLHandle(); //!< virtual destructor

        CURLHandle &operator=(const CURLHandle &rhs) = delete; //!< Deleted copy assignment operator
        CURLHandle(const CURLHandle &rhs) = delete; //!< Deleted copy constructor

        /**
         * @brief Move copy operator. NOTE: This operator MAY NOT be used when a `perform()` task is active.
         * @param rhs Right Hand Side of the `std::move`
         * @return A reference to this object
         */
        CURLHandle &operator=(CURLHandle &&rhs) noexcept;

        /**
         * @brief Move copy constructor. NOTE: This constructor MAY NOT be used when a `perform()` task is active.
         * @param rhs Right Hand Side of the `std::move`
         */
        CURLHandle(CURLHandle &&rhs) noexcept;

        /**
         * @brief Set the target URL we will connect to
         * @param cUrl URL to connect to
         */
        inline void setUrl(const char *cUrl) {
            curl_easy_setopt(handle, CURLOPT_URL, cUrl);
            url = std::string(cUrl);
        }

        /**
         * @brief Get the target URL
         * @return The URL as a string
         */
        inline std::string getUrl() {
            return url;
        }

        /**
         * @brief  perform the operation that was configured. This function is async and will return right away
         *         after submitting a task to the `ThreadPool`.
         * @return See libcurl documentation for `CURLcode`. You can treat is as a bool that is false if
         *         the operation was successful, true otherwise.
         */
        std::future<CURLcode> perform();


    };
}

#endif //__STONEMASON_CURL_HPP
