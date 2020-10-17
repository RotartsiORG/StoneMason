//
// Created by grant on 4/21/20.
//

#pragma once

#ifndef __STONEMASON_UTIL_HPP
#define __STONEMASON_UTIL_HPP

#include <string>
#include <random>

#include <openssl/rand.h>
#include "openssl/ssl.h"

namespace stms {

    constexpr char hexChars[] = "0123456789abcdef";

    inline std::default_random_engine &stmsRand() {
        static std::random_device seedGen = std::random_device();
        static std::default_random_engine ret(seedGen());
        return ret;
    }

    /**
     * @brief Read the contents of a file
     * @throw If `stms::exceptionLevel` > 0, an `std::runtime_error` is thrown if `filename` cannot be read.
     * @param filename File to read
     * @return Contents of file as string.
     */
    std::string readFile(const std::string &filename);

    enum UUIDType {
        eUninitialized,
        eUuid4
    };

    inline int sslRand(unsigned char * buf, int size) {
        return RAND_bytes(buf, size);
    }

    std::string toHex(unsigned long long in, uint8_t places = 0);

    struct UUID {
    public:
        uint32_t timeLow{};
        uint16_t timeMid{};
        uint16_t timeHiAndVersion{};
        uint8_t clockSeqHiAndReserved{};
        uint8_t clockSeqLow{};
        uint8_t node[6]{};

        [[nodiscard]] std::string buildStr() const;

        UUID() = default;

        explicit UUID(UUIDType type);

        UUID(const UUID &rhs);

        UUID &operator=(const UUID &rhs);

        bool operator==(const UUID &rhs) const;

        bool operator!=(const UUID &rhs) const;
    };

    UUID genUUID4();

    int intRand(int min, int max);

    float floatRand(float min, float max);

    class _stms_STMSInitializer {

    public:
        static bool hasRun;

        uint8_t specialValue;

        _stms_STMSInitializer() noexcept;

        ~_stms_STMSInitializer() noexcept;

        _stms_STMSInitializer(_stms_STMSInitializer &rhs) = delete;

        _stms_STMSInitializer &operator=(_stms_STMSInitializer &rhs) = delete;
    };

    inline _stms_STMSInitializer &_stms_getInitializer() {
        static _stms_STMSInitializer val = _stms_STMSInitializer();
        return val;
    };

    void initAll();
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


#endif //__STONEMASON_UTIL_HPP
