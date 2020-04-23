//
// Created by grant on 4/21/20.
//

#include "stms/util.hpp"
#include <bitset>
#include "spdlog/fmt/fmt.h"
#include "openssl/rand.h"
#include "openssl/ssl.h"
#include "openssl/err.h"
#include "stms/logging.hpp"

namespace stms {
    bool STMSInitializer::hasRun = false;

    static std::random_device __seedGen;
    std::default_random_engine stmsRand(__seedGen());

    int (*sslRand)(uint8_t *, int) = RAND_bytes;

    int (*privateSslRand)(uint8_t *, int) = RAND_priv_bytes;

    int intRand(int min, int max) {
        std::uniform_int_distribution<> dist(min, max);
        return dist(stmsRand);
    }

    float floatRand(float min, float max) {
        std::uniform_real_distribution<> dist(min, max);
        return dist(stmsRand);
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

    STMSInitializer stmsInitializer;

    void initAll() {
        std::cout << stmsInitializer.specialValue;
    }

    std::string UUID::getStr() {
        strCache = fmt::format("{:08x}-{:04x}-{:04x}-{:02x}{:02x}-", timeLow, timeMid, timeHiAndVersion,
                               clockSeqHiAndReserved, clockSeqLow);
        for (uint8_t i : node) {
            strCache += fmt::format("{:02x}", static_cast<unsigned>(i));
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
        strCache = rhs.strCache;
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

    STMSInitializer::STMSInitializer() noexcept {
        if (hasRun) {
            STMS_INFO("The constructor for `stms::STMSInitializer` has been called twice! This is not intended!");
            STMS_INFO(
                    "This is most likely because YOU, the STMS user, has tried to initialize another STMSInitializer!");
            STMS_INFO("Don't do that! This invocation will be ignored, but you BETTER fix it... :(");
            return;
        }
        stms::logging.init();

        // OpenSSL initialization
        SSL_library_init();
        SSL_load_error_strings();
        OpenSSL_add_all_algorithms();
        ERR_load_crypto_strings();

        int pollStatus = RAND_poll();
        if (pollStatus != 1 || RAND_status() != 1) {
            STMS_CRITICAL("[* FATAL ERROR *] Unable to seed OpenSSL RNG with enough random data!");
            STMS_CRITICAL(
                    "Using this insecure RNG will result in UUID collisions and insecure cryptographic key generation!");
            if (!STMS_IGNORE_BAD_RNG) {
                STMS_CRITICAL(
                        "Aborting! (Compile with `STMS_IGNORE_BAD_RNG` in `config.hpp` set to `true` to force the use of the insecure RNG)");
                exit(1);
            } else {
                STMS_CRITICAL("!!! Using insecure RNG! Set `STMS_IGNORE_BAD_RNG` to `false` in `config.hpp` !!!");
            }
        }
    }
}