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
    extern char hexDigits[16];

    struct UUID {
    public:
        uint32_t timeLow;
        uint16_t timeMid;
        uint16_t timeHiAndVersion;
        uint8_t clockSeqHiAndReserved;
        uint8_t clockSeqLow;
        uint8_t node[6];

        std::string strCache;

        std::string getStr();
    };

    UUID genUUID4();

    int intRand(int min, int max);

    float floatRand(float min, float max);
}

#endif //__STONEMASON_UTIL_HPP
