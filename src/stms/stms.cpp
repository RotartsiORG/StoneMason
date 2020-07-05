//
// Created by grant on 4/21/20.
//

#include "stms/stms.hpp"
#include <bitset>

#include "openssl/rand.h"
#include "stms/net/ssl.hpp"

#include "stms/logging.hpp"

#include "stms/curl.hpp"

#include "stms/config.hpp"
#ifdef STMS_ENABLE_OPENGL
#    include "stms/rend/gl/gl.hpp"
#endif

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
        int status = RAND_bytes(reinterpret_cast<unsigned char *>(&uuid), 16); // UUIDs have 128 bits (16 octets).
        if (status != 1) {
            STMS_WARN("Failed to randomly generate UUID using OpenSSL! Using less secure C++ stdlib random instead");
            std::uniform_int_distribution<uint64_t> u64Dist(0UL, UINT64_MAX);
            auto *lowerHalf = reinterpret_cast<uint64_t *>(&uuid);
            *lowerHalf = u64Dist(stmsRand());
            *(lowerHalf + 1) = u64Dist(stmsRand());
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

        ret += std::accumulate(std::begin(node), std::end(node), std::string("-"), [](std::string a, uint8_t b) {
            return std::move(a) + toHex(b, 2);
        });

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
               (std::memcmp(node, rhs.node, sizeof(uint8_t) * 6) == 0);
    }

    bool UUID::operator!=(const UUID &rhs) const {
        return !(*this == rhs);
    }

    UUID::UUID(UUIDType type) {
        switch (type) {
            case (eUuid4): {
                *this = genUUID4();
                break;
            }

            default: {
                // no-op
                break;
            }
        }
    }

    _stms_STMSInitializer::_stms_STMSInitializer() noexcept: specialValue(0) {
        if (hasRun) {
            STMS_WARN("_stms_STMSInitializer was called more than once! Ignoring invocation!");
            return;
        }

        hasRun = true;

        stms::initLogging();

        stms::initOpenSsl();

#ifdef STMS_ENABLE_OPENGL
        stms::initGl();
#endif

        stms::initCurl();
    }

    _stms_STMSInitializer::~_stms_STMSInitializer() noexcept {

#ifdef STMS_ENABLE_OPENGL
        stms::quitGl();
#endif

        stms::quitOpenSsl();

        stms::quitCurl();

        stms::quitLogging();
    }
}

// https://www.boost.org/doc/libs/1_73_0/boost/container_hash/hash.hpp
static size_t boostHashCombine(size_t lhs, size_t rhs) {
    return lhs ^ (rhs + 0x9e3779b9 + (lhs << 6UL) + (lhs >> 2UL));
}

namespace std {
    template<> struct hash<stms::UUID> {
        std::size_t operator()(stms::UUID const& s) const noexcept {
            auto *lowerHalf = reinterpret_cast<const uint64_t *>(&s);
            return boostHashCombine(*lowerHalf, *(lowerHalf + 1));
        }
    };
}
