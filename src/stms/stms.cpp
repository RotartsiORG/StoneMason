//
// Created by grant on 4/21/20.
//

#include "stms/stms.hpp"
#include <bitset>

#include "openssl/rand.h"
#include "stms/net/ssl.hpp"

#include "stms/logging.hpp"
#include "stms/curl.hpp"
#include "stms/rend/window.hpp"
#include "stms/config.hpp"

namespace stms {
    _stms_STMSInitializer _stms_initializer = _stms_STMSInitializer();
    bool _stms_STMSInitializer::hasRun = false;

    void initAll() {
        fputc(_stms_initializer.specialValue, stdout); // Stop -O3 from optimizing the initializer out ¯\_(ツ)_/¯
    }

    _stms_STMSInitializer::_stms_STMSInitializer() noexcept: specialValue(0) {
        if (hasRun) {
            STMS_WARN("_stms_STMSInitializer was called more than once! Ignoring invocation!");
            return;
        }

        hasRun = true;

        stms::initLogging();

        stms::initOpenSsl();

        stms::initGlfw();

        stms::initCurl();
    }

    _stms_STMSInitializer::~_stms_STMSInitializer() noexcept {

        stms::quitGlfw();

        stms::quitOpenSsl();

        stms::quitCurl();

        stms::quitLogging();
    }
}
