//
// Created by grant on 12/27/20.
//

#include "stms/util/uuid.hpp"

#include <openssl/rand.h>
#include "openssl/ssl.h"

#include "stms/logging.hpp"

#include <cstring>

namespace stms {

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
            case (UUIDType::eUuid4): {
                *this = genUUID4();
                break;
            }

            default: {
                // no-op
                break;
            }
        }
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

        /* RFC 4122
         * "Set the four most significant bits (bits 12 through 15) of the
         * time_hi_and_version field to the 4-bit version number from
         * Section 4.1.3."
         *
         * The version number is in the most significant 4 bits of the time
         * stamp (bits 4 through 7 of the time_hi_and_version field).
         *
         * Msb0  Msb1  Msb2  Msb3
         *  0     1     0     0
         */

        uuid.timeHiAndVersion &= (65535u ^ ((1u << 15u) | (1u << 13u) | (1u << 12u)));
        uuid.timeHiAndVersion |= (1u << 14u);


        /* RFC 4122
         * "Set the two most significant bits (bits 6 and 7) of the
         * clock_seq_hi_and_reserved to zero and one, respectively."
         */
        uuid.clockSeqHiAndReserved &= (255u ^ (1u << 6u)); // Set 6th bit to 0 by &= 0b10111111
        uuid.clockSeqHiAndReserved |= (1u << 7u);          // Set 7th bit to 1 by |= 0b10000000

        return uuid;
    }
}

