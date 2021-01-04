//
// Created by grant on 12/27/20.
//

#pragma once

#ifndef __STONEMASON_UUID_HPP
#define __STONEMASON_UUID_HPP

#include <random>
#include "stms/util/util.hpp"

namespace stms {

    /// UUID type, or version. In the future, other types of UUIDs may be supported (in addition to UUIDv4)
    enum class UUIDType : uint8_t {
        eUninitialized, //!< Used for UUIDs where all 128 bits are set to 0
        eUuid4 //!< Used for UUIDv4s, where 124 bits are randomly set. See RFC 4122 for more info
    };

    /// 128-bit Universally Unique Identifier. See RFC 4122
    struct UUID {
    public:
        uint32_t timeLow{}; //!< Low field of timestamp. Called `time_low` in the RFC
        uint16_t timeMid{}; //!< Mid field of timestamp. Called `time_mid` in the RFC
        uint16_t timeHiAndVersion{}; //!< High field of timestamp multiplex with version number. `time_hi_and_version`
        uint8_t clockSeqHiAndReserved{}; //!< High field of clock sequence multiplexed with the variant.
        uint8_t clockSeqLow{}; //!< Low field of the clock sequence. `clock_seq_low` in RFC
        uint8_t node[6]{}; //!< Spatially unique node identifier. Unsigned 48-bit int. Called `node` in the RFC.

        /**
         * @brief Convert this UUID to a human-readable hex string, as specified in RFC 4122
         * @return 36-character long string.
         */
        [[nodiscard]] std::string buildStr() const;

        UUID() = default; //!< Initializes UUID to all 128 bits being 0s

        /**
         * @brief Normal constructor for UUIDs
         * @param type Type of UUID to initialize.
         *             Use `eUninitialized` for all 0s
         *             Use `eUuid4` for random 124 bits. See RFC 4122 for UUIDv4. Equivalent to `genUUID4`
         */
        explicit UUID(UUIDType type);

        UUID(const UUID &rhs); //!< Cpy c'tor

        UUID &operator=(const UUID &rhs); //!< cpy op

        bool operator==(const UUID &rhs) const; //!< Equality op

        bool operator!=(const UUID &rhs) const; //!< Ineq op
    };

    /**
     * @brief Generate a UUIDv4. See RFC 4122. Equivalent to `UUID(eUuid4)`
     * @return Generated UUID
     */
    UUID genUUID4();
}


/// Namespace full of the nastiest sexually transmitted diseases (JK it's what you think it is))
namespace std {

    /// Hash function for `stms::UUID`s so that they can be used in maps and stuff.
    template<> struct hash<stms::UUID> {

        /**
         * @brief Implementation of the hash function. This simply calls `stms::boostHashCombine`
         *        on the lower and upper 64 bits of the UUID
         * @param s UUID to hash
         * @return Combined lower and upper halves of the UUID
         */
        std::size_t operator()(stms::UUID const& s) const noexcept {
            auto *lowerHalf = reinterpret_cast<const uint64_t *>(&s);
            return stms::boostHashCombine(*lowerHalf, *(lowerHalf + 1));
        }
    };
}


#endif //STMS_STATIC_UUID_HPP
