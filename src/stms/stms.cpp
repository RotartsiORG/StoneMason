//
// Created by grant on 4/21/20.
//

#include "stms/stms.hpp"
#include <bitset>
#include <fstream>

#include "openssl/rand.h"
#include "stms/net/ssl.hpp"

#include "stms/logging.hpp"
#include "stms/curl.hpp"
#include "stms/rend/window.hpp"
#include "stms/config.hpp"

namespace stms {
    _stms_STMSInitializer _stms_initializer = _stms_STMSInitializer();
    bool _stms_STMSInitializer::hasRun = false;

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

    void initAll() {
        fputc(_stms_initializer.specialValue, stdout); // Stop -O3 from optimizing the initializer out ¯\_(ツ)_/¯
    }

    std::string toHex(unsigned long long in, uint8_t places) {
        std::string ret;
        ret.reserve(places);

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

    std::string readFile(const std::string &filename) {
        // Open in binary mode, this might cause issues with line endings but fuck windows
        std::ifstream in(filename, std::ios::ate | std::ios::binary);
        if (!in.is_open()) {
            STMS_WARN("Unable to open {}!", filename);
            if (exceptionLevel > 0) {
                throw std::runtime_error(fmt::format("Unable to open '{}'", filename));
            }
            return "";
        }

        std::string str;
        str.reserve(in.tellg());

        in.seekg(0, std::ios::beg);

        str.assign((std::istreambuf_iterator<char>(in)),
                   std::istreambuf_iterator<char>());
        return str;
    }

    size_t boostHashCombine(size_t lhs, size_t rhs) {
        return lhs ^ (rhs + 0x9e3779b9 + (lhs << 6UL) + (lhs >> 2UL));
    }

    std::string UUID::buildStr() const {
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
        if (this == &rhs || rhs == *this) {
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
