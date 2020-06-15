//
// Created by grant on 4/21/20.
//

#include "stms/stms.hpp"
#include <bitset>

#include "openssl/rand.h"
#include "stms/net/ssl.hpp"

#include "stms/rend/gl/gl.hpp"

#include "stms/logging.hpp"
#include "stms/audio.hpp"

#include "stms/curl.hpp"

namespace stms {
    bool _stms_STMSInitializer::hasRun = false;
    int(*sslRand)(unsigned char *, int) = RAND_bytes;

    int intRand(int min, int max) {
        std::uniform_int_distribution<> dist(min, max);
        return dist(stmsRand());
    }

    float floatRand(float min, float max) {
        std::uniform_real_distribution<> dist(min, max);
        return dist(stmsRand());
    }

    UUID genUUID4() {
        UUID uuid{};
        int status = RAND_bytes(reinterpret_cast<unsigned char *>(&uuid),
                                16); // UUIDs have 128 bits (16 bytes). This ignores the cacheString attrib
        if (status != 1) {
            STMS_PUSH_WARNING("Failed to randomly generate UUID using OpenSSL! Using less secure C++ stdlib random instead");
            uuid.timeLow = intRand(0, UINT32_MAX);
            uuid.timeMid = intRand(0, UINT16_MAX);
            uuid.timeHiAndVersion = intRand(0, UINT16_MAX);
            uuid.clockSeqHiAndReserved = intRand(0, UINT8_MAX);
            uuid.clockSeqLow = intRand(0, UINT8_MAX);

            std::generate(std::begin(uuid.node), std::end(uuid.node), []() {
                return intRand(0, UINT8_MAX);
            });
        }

        uuid.timeHiAndVersion &= (65535u ^ ((1u << 15u) | (1u << 13u) | (1u << 12u)));
        uuid.timeHiAndVersion |= (1u << 14u);
        uuid.clockSeqHiAndReserved &= (255u ^ (1u << 6u));
        uuid.clockSeqHiAndReserved |= (1u << 7u);

        return uuid;
    }

    _stms_STMSInitializer stmsInitializer = _stms_STMSInitializer();

    void initAll() {
        std::cout << stmsInitializer.specialValue;
    }

    std::string toHex(unsigned long long in, uint8_t places) {
        std::string ret;

        uint8_t i = 0;
        uint8_t lastAdded = in % 16;

        // Even if i > places, it will keep going until we get a 0 so that we retain 100% of the data
        // Is this behaviour good?
        while (lastAdded != 0 || i < places) {
            ret += hexChars[lastAdded];
            lastAdded = (in / static_cast<unsigned long long>(std::pow(16, ++i))) % 16;
        }

        std::reverse(ret.begin(), ret.end());
        return ret;
    }

    std::string UUID::buildStr() {
        strCache.clear();
        strCache.reserve(36);

        strCache += toHex(timeLow, 8);
        strCache += '-';
        strCache += toHex(timeMid, 4);
        strCache += '-';
        strCache += toHex(timeHiAndVersion, 4);
        strCache += '-';
        strCache += toHex(clockSeqHiAndReserved, 2);
        strCache += toHex(clockSeqLow, 2);
        strCache += '-';

        for (uint8_t i : node) {
            strCache += toHex(i, 2);
        }

        return strCache;
    }

    UUID::UUID(const UUID &rhs) {
        *this = rhs;
    }

    UUID &UUID::operator=(const UUID &rhs) {
        if (this == &rhs) {
            return *this;
        }

        timeLow = rhs.timeLow;
        timeMid = rhs.timeMid;
        timeHiAndVersion = rhs.timeHiAndVersion;
        clockSeqLow = rhs.clockSeqLow;
        clockSeqHiAndReserved = rhs.clockSeqHiAndReserved;
        std::copy(std::begin(rhs.node), std::end(rhs.node), std::begin(node));

        strCache = rhs.strCache;

        return *this;
    }

    bool UUID::operator==(const UUID &rhs) const {
        return (timeLow == rhs.timeLow) && (timeMid == rhs.timeMid) && (timeHiAndVersion == rhs.timeHiAndVersion) &&
               (clockSeqLow == rhs.clockSeqLow) && (clockSeqHiAndReserved == rhs.clockSeqHiAndReserved) &&
               (std::memcmp(node, rhs.node, sizeof(uint8_t) * 6) == 0);
    }

    bool UUID::operator!=(const UUID &rhs) const {
        return !(*this == rhs);
    }

    _stms_STMSInitializer::_stms_STMSInitializer() noexcept: specialValue(0) {
        if (hasRun) {
            STMS_PUSH_WARNING("_stms_STMSInitializer was called more than once! Ignoring invocation!")
            return;
        }

        hasRun = true;

        stms::initLogging();

        net::initOpenSsl();

        stms::rend::initGl();

        stms::initCurl();

        stms::al::defaultAlContext();  // Initialize openal by requesting the default context
    }
}