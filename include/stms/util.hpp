//
// Created by grant on 4/21/20.
//

#pragma once

#ifndef __STONEMASON_UTIL_HPP
#define __STONEMASON_UTIL_HPP

#include <string>
#include <random>

namespace stms {
    extern std::default_random_engine stmsRand;

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

        uint8_t specialValue = 0;

        STMSInitializer() noexcept;

        STMSInitializer(STMSInitializer &rhs) = delete;

        STMSInitializer &operator=(STMSInitializer &rhs) = delete;

        static void initOpenSsl();
    };

    extern STMSInitializer stmsInitializer;

    void initAll();
}

#endif //__STONEMASON_UTIL_HPP
