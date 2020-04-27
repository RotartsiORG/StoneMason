//
// Created by grant on 4/21/20.
//

#include "stms/util.hpp"
#include <bitset>
#include "spdlog/fmt/fmt.h"
#include "openssl/rand.h"
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
            STMS_INFO("This is likely because a second STMSInitializer was created by non-STMS code.");
            STMS_INFO("This invocation will be ignored.");
            return;
        }
        stms::initLogging();

        initOpenSsl();
    }

    void STMSInitializer::initOpenSsl() {
        // OpenSSL initialization
        if (OpenSSL_version_num() != OPENSSL_VERSION_NUMBER) {
            STMS_CRITICAL("[* FATAL ERROR *] OpenSSL version mismatch! "
                          "Linked and compiled/included OpenSSL versions are not the same!");
            STMS_CRITICAL("Compiled/Included OpenSSL {} while linked OpenSSL {}",
                          OPENSSL_VERSION_TEXT, OpenSSL_version(OPENSSL_VERSION));
            if (OpenSSL_version_num() >> 20 != OPENSSL_VERSION_NUMBER >> 20) {
                STMS_CRITICAL("OpenSSL major and minor version numbers do not match. Aborting.");
                // TODO: implement
                STMS_CRITICAL("Set `STMS_IGNORE_SSL_MISMATCH` to `true` in `config.hpp` to ignore this error.");
                STMS_CRITICAL("Only do this if you plan on NEVER using OpenSSL functionality.");
                exit(1);
            }
        }

        if (OPENSSL_VERSION_NUMBER < 0x1010102fL) {
            STMS_CRITICAL("OpenSSL{} is outdated and insecure! OpenSSL 1.1.1a is required!");
            // TODO: implement
            STMS_CRITICAL("Set `STMS_IGNORE_OLD_SSL` to `true` in `config.hpp` to ignore this error.");
            STMS_CRITICAL("Only do this if you plan on NEVER using OpenSSL functionality.");
            exit(1);
        }

        int pollStatus = RAND_poll();
        if (pollStatus != 1 || RAND_status() != 1) {
            STMS_CRITICAL("[* FATAL ERROR *] Unable to seed OpenSSL RNG with enough random data!");
            STMS_CRITICAL("OpenSSL may generate insecure cryptographic keys, and UUID collisions may occur");
            if (!STMS_IGNORE_BAD_RNG) {
                STMS_CRITICAL("Aborting! Set `STMS_IGNORE_BAD_RNG` to ignore this fatal error.");
                STMS_CRITICAL("Only do this if you plan on NEVER using OpenSSL functionality.");
                exit(1);
            } else {
                STMS_CRITICAL("Using insecure RNG! Only do this if you plan on NEVER using OpenSSL functionality.");
                STMS_CRITICAL("Otherwise, set `STMS_IGNORE_BAD_RNG` to `false` in `config.hpp`");
            }
        }

        STMS_INFO("Initialized OpenSSL {}!", OpenSSL_version(OPENSSL_VERSION));
    }
}