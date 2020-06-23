//
// Created by grant on 6/2/20.
//

#include "stms/curl.hpp"

#include <mutex>
#include "stms/logging.hpp"

namespace stms {

    struct _stms_CurlWriteUserDatType {
        CurlResult *res = nullptr;
        std::mutex strMtx;
    };

    static size_t curlWriteCb(void *buf, size_t size, size_t nmemb, void *userDat) {
        auto dat = reinterpret_cast<_stms_CurlWriteUserDatType *>(userDat);

        std::lock_guard<std::mutex> lg(dat->strMtx);
        dat->res->data.append(reinterpret_cast<char *>(buf), size * nmemb);
        return size * nmemb;
    }

    static int curlProgressCb(void *userDat, double dTotal, double dNow, double uTotal, double uNow) {
        auto dat = reinterpret_cast<_stms_CurlWriteUserDatType *>(userDat);
        dat->res->downloadTotal = dTotal;
        dat->res->downloadNow = dNow;

        dat->res->uploadNow = uNow;
        dat->res->uploadTotal = uTotal;
        return 0;
    }

    std::future<CurlResult> readURL(const char *url, ThreadPool *pool, unsigned priority) {
        std::shared_ptr<std::promise<CurlResult>> prom = std::make_shared<std::promise<CurlResult>>();

        pool->submitTask([=](void *) -> void * {
            auto handle = curl_easy_init();
            curl_easy_setopt(handle, CURLOPT_URL, url);

            CurlResult res;
            _stms_CurlWriteUserDatType userDat;
            userDat.res = &res;

            curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, curlWriteCb);
            curl_easy_setopt(handle, CURLOPT_WRITEDATA, &userDat);

            curl_easy_setopt(handle, CURLOPT_NOPROGRESS, 0);
            curl_easy_setopt(handle, CURLOPT_PROGRESSFUNCTION, curlProgressCb);
            curl_easy_setopt(handle, CURLOPT_PROGRESSDATA, &userDat);

            auto err = curl_easy_perform(handle);
            curl_easy_cleanup(handle);

            if (err != CURLE_OK) {
                STMS_PUSH_ERROR("Failed to read url {}", url);
                res.success = false;
                prom->set_value(res);
                return nullptr;
            }

            res.success = true;
            prom->set_value(res);
            return nullptr;
        }, nullptr, priority);

        return prom->get_future();
    }

    void initCurl() {
        curl_global_init(CURL_GLOBAL_SSL | CURL_GLOBAL_WIN32);
        auto version = curl_version_info(CURLVERSION_NOW);
        STMS_INFO("LibCURL {}: On host '{}' with libZ {} & SSL {}", version->version, version->host, version->libz_version, version->ssl_version);
    }

    void quitCurl() {
        curl_global_cleanup();
    }
}

