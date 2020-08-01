//
// Created by grant on 4/21/20.
//

#pragma once

#ifndef __STONEMASON_UTIL_HPP
#define __STONEMASON_UTIL_HPP

#include <string>
#include <random>

#include "openssl/ssl.h"

namespace stms {

    constexpr char hexChars[] = "0123456789abcdef";

    inline std::default_random_engine &stmsRand() {
        static std::random_device seedGen = std::random_device();
        static std::default_random_engine ret(seedGen());
        return ret;
    }

    enum UUIDType {
        eUninitialized,
        eUuid4
    };

    extern int(*sslRand)(unsigned char *, int);

    std::string toHex(unsigned long long in, uint8_t places = 0);

    struct UUID {
    public:
        uint32_t timeLow{};
        uint16_t timeMid{};
        uint16_t timeHiAndVersion{};
        uint8_t clockSeqHiAndReserved{};
        uint8_t clockSeqLow{};
        uint8_t node[6]{};

        std::string buildStr();

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

    extern _stms_STMSInitializer stmsInitializer;

    void initAll();

    inline bool getBit(uint64_t in, uint8_t bit) {
        return in & (1u << bit);
    }

#define __STMS_UNSET_FUNC(numBits) inline uint##numBits##_t resetBit##numBits(uint##numBits##_t in, uint8_t bit) { return in & (UINT##numBits##_MAX ^ (1u << bit)); }

    __STMS_UNSET_FUNC(8)

    __STMS_UNSET_FUNC(16)

    __STMS_UNSET_FUNC(32)

    __STMS_UNSET_FUNC(64)

#undef __STMS_UNSET_FUNC

    inline uint64_t setBit(uint64_t in, uint8_t bit) {
        return in | (1u << bit);
    }

}

#endif //__STONEMASON_UTIL_HPP
