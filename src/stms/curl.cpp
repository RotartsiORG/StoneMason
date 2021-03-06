//
// Created by grant on 6/2/20.
//

#include "stms/curl.hpp"

#include <mutex>
#include "stms/logging.hpp"

namespace stms {
    void initCurl() {
        curl_global_init(CURL_GLOBAL_SSL | CURL_GLOBAL_WIN32);
        auto version = curl_version_info(CURLVERSION_NOW);
        STMS_INFO("LibCURL {}: On host '{}' with libZ {} & SSL {}", version->version, version->host, version->libz_version, version->ssl_version);
    }

    void quitCurl() {
        curl_global_cleanup();
    }

    static size_t curlWriteCb(void *buf, size_t size, size_t nmemb, void *userDat) {
        auto dat = reinterpret_cast<CURLHandle *>(userDat);

        std::lock_guard<std::mutex> lg(dat->resultMtx);
        dat->result.append(reinterpret_cast<char *>(buf), size * nmemb);
        return size * nmemb;
    }

    static int curlProgressCb(void *userDat, double dTotal, double dNow, double uTotal, double uNow) {
        auto dat = reinterpret_cast<CURLHandle *>(userDat);
        dat->downloadTotal = dTotal;
        dat->downloadSoFar = dNow;

        dat->uploadSoFar = uNow;
        dat->uploadTotal = uTotal;
        return 0;
    }

    CURLHandle::CURLHandle(PoolLike *inPool) : pool(inPool), handle(curl_easy_init()) {
        curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, curlWriteCb);
        curl_easy_setopt(handle, CURLOPT_WRITEDATA, this);

        curl_easy_setopt(handle, CURLOPT_NOPROGRESS, 0);
        curl_easy_setopt(handle, CURLOPT_PROGRESSFUNCTION, curlProgressCb);
        curl_easy_setopt(handle, CURLOPT_PROGRESSDATA, this);
    }

    CURLHandle::~CURLHandle() {
        curl_easy_cleanup(handle);
        handle = nullptr;
    }

    std::future<CURLcode> CURLHandle::perform() {
        result.clear(); // Should we require the client to explicitly request this?
        std::shared_ptr<std::promise<CURLcode>> prom = std::make_shared<std::promise<CURLcode>>();

        pool.load()->submitTask([=]() {
            auto err = curl_easy_perform(handle); // This is typically an expensive call so we async it
            if (err != CURLE_OK) {
                STMS_ERROR("Failed to perform curl operation on url {}", url); // We can't throw an exception so :[
            }

            prom->set_value(err);
        });

        return prom->get_future();
    }

    CURLHandle &CURLHandle::operator=(CURLHandle &&rhs) noexcept {
        if (&rhs == this || rhs.handle.load() == handle.load()) {
            return *this;
        }

        std::lock_guard<std::mutex> rlg(rhs.resultMtx);
        std::lock_guard<std::mutex> tlg(resultMtx);
        result = std::move(rhs.result);

        downloadSoFar = rhs.downloadTotal;
        downloadTotal = rhs.downloadTotal;

        uploadSoFar = rhs.uploadSoFar;
        uploadTotal = rhs.uploadTotal;

        pool = rhs.pool.load();
        handle = rhs.handle.load();
        url = std::move(rhs.url);

        return *this;
    }

    CURLHandle::CURLHandle(CURLHandle &&rhs) noexcept : handle(nullptr) {
        *this = std::move(rhs);
    }
}

