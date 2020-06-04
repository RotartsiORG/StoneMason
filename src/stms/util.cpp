//
// Created by grant on 4/21/20.
//

#include "stms/util.hpp"
#include <bitset>

#include "openssl/rand.h"
#include "stms/net/ssl.hpp"

#include "stms/rend/gl/gl.hpp"

#include "stms/logging.hpp"
#include "stms/audio.hpp"

#include "stms/curl.hpp"

namespace stms {
    bool STMSInitializer::hasRun = false;

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
            STMS_INFO("Failed to randomly generate UUID using OpenSSL! Using less secure C++ stdlib random instead");
            uuid.timeLow = intRand(0, UINT32_MAX);
            uuid.timeMid = intRand(0, UINT16_MAX);
            uuid.timeHiAndVersion = intRand(0, UINT16_MAX);
            uuid.clockSeqHiAndReserved = intRand(0, UINT8_MAX);
            uuid.clockSeqLow = intRand(0, UINT8_MAX);
            for (uint8_t &i : uuid.node) {
                i = intRand(0, UINT8_MAX);
            }
        }

        uuid.timeHiAndVersion &= (65535u ^ ((1u << 15u) | (1u << 13u) | (1u << 12u)));
        uuid.timeHiAndVersion |= (1u << 14u);
        uuid.clockSeqHiAndReserved &= (255u ^ (1u << 6u));
        uuid.clockSeqHiAndReserved |= (1u << 7u);

        return uuid;
    }

    STMSInitializer stmsInitializer = STMSInitializer();

    void initAll() {
        std::cout << stmsInitializer.specialValue;
    }

    template<typename T>
    std::string toHex(T in, uint8_t places) {
        std::string ret;

        uint8_t i = 0;
        uint8_t lastAdded = in % 16;

        // Even if i > places, it will keep going until we get a 0 so that we retain 100% of the data
        // Is this behaviour good?
        while (lastAdded != 0 || i < places) {
            ret += hexChars[lastAdded];
            lastAdded = static_cast<int>(in / std::pow(16, ++i)) % 16;
        }

        std::reverse(ret.begin(), ret.end());
        return ret;
    }

    std::string UUID::getStr() {
        std::string ret;
        ret.reserve(36);

        ret += toHex(timeLow, 8);
        ret += '-';
        ret += toHex(timeMid, 4);
        ret += '-';
        ret += toHex(timeHiAndVersion, 4);
        ret += '-';
        ret += toHex(clockSeqHiAndReserved, 2);
        ret += toHex(clockSeqLow, 2);
        ret += '-';

        for (uint8_t i : node) {
            ret += toHex(i, 2);
        }

        return ret;
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

        return *this;
    }

    bool UUID::operator==(const UUID &rhs) const {
        return (timeLow == rhs.timeLow) && (timeMid == rhs.timeMid) && (timeHiAndVersion == rhs.timeHiAndVersion) &&
               (clockSeqLow == rhs.clockSeqLow) && (clockSeqHiAndReserved == rhs.clockSeqHiAndReserved) &&
               (memcmp(node, rhs.node, sizeof(uint8_t) * 6) == 0);
    }

    bool UUID::operator!=(const UUID &rhs) const {
        return !(*this == rhs);
    }

    STMSInitializer::STMSInitializer() noexcept: specialValue(0) {
        if (hasRun) {
            STMS_INFO("The constructor for `stms::STMSInitializer` has been called twice! This is not intended!");
            STMS_INFO("This is likely because a second STMSInitializer was created by non-STMS code.");
            STMS_INFO("This invocation will be ignored.");
            return;
        }
        stms::initLogging();

        net::initOpenSsl();

        stms::rend::initGl();

        stms::initCurl();

        stms::al::defaultAlContext();  // Initialize openal
    }
}