//
// Created by grant on 4/21/20.
//

#include "stms/stms.hpp"
#include <bitset>

#include "stms/net/ssl.hpp"

#include "stms/logging.hpp"
#include "stms/curl.hpp"
#include "stms/rend/window.hpp"
#include "stms/config.hpp"

namespace stms {
    static inline uint8_t *&getEmergencyMemoryBlock() {
        static uint8_t *val = nullptr;
        return val;
    }

    static void newHandler() {
        delete[] getEmergencyMemoryBlock();
        getEmergencyMemoryBlock() = nullptr;

        std::set_new_handler(nullptr);

        STMS_FATAL("[OOM] Out of memory! Freeing initial emergency memory block! Expect a crash!");
    }


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

        std::set_new_handler(newHandler); // set new handler bf new
        getEmergencyMemoryBlock() = new uint8_t[emergencyMemBlockSize];

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

        std::set_new_handler(nullptr);
        delete[] getEmergencyMemoryBlock();
        getEmergencyMemoryBlock() = nullptr;
    }
}
