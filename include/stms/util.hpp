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

    inline std::default_random_engine &stmsRand() {
        static std::random_device seedGen = std::random_device();
        static std::default_random_engine ret(seedGen());
        return ret;
    }

    extern int (*sslRand)(uint8_t *, int);

    extern int (*privateSslRand)(uint8_t *, int);

    struct UUID {
    public:
        uint32_t timeLow{};
        uint16_t timeMid{};
        uint16_t timeHiAndVersion{};
        uint8_t clockSeqHiAndReserved{};
        uint8_t clockSeqLow{};
        uint8_t node[6]{};

        std::string strCache;

        std::string getStr();

        UUID() = default;

        UUID(const UUID &rhs);

        UUID &operator=(const UUID &rhs);

        bool operator==(const UUID &rhs) const;

        bool operator!=(const UUID &rhs) const;
    };

    UUID genUUID4();

    int intRand(int min, int max);

    float floatRand(float min, float max);

    class STMSInitializer {

    public:
        static bool hasRun;

        uint8_t specialValue;

        STMSInitializer() noexcept;

        STMSInitializer(STMSInitializer &rhs) = delete;

        STMSInitializer &operator=(STMSInitializer &rhs) = delete;
    };

    extern STMSInitializer stmsInitializer;

    void initAll();

    template<typename T>
    class _stms_flag {
    private:
        T state = 0;
    public:
        _stms_flag() = default;

        _stms_flag(const _stms_flag &rhs) = default;

        _stms_flag &operator=(const _stms_flag &rhs) = default;

        inline void setBit(T bit, bool on) {
            if (on) {
                state |= (1u << bit);
            } else {
                state &= (255u ^ (1u << bit));
            }
        }

        [[nodiscard]] inline bool getBit(T bit) const {
            return state & (1u << bit);
        }
    };

    typedef _stms_flag<uint8_t> Flag8Bit;
    typedef _stms_flag<uint16_t> Flag16Bit;
    typedef _stms_flag<uint32_t> Flag32Bit;
    typedef _stms_flag<uint64_t> Flag64Bit;
}

#endif //__STONEMASON_UTIL_HPP
