//
// Created by grant on 4/21/20.
//

#include "stms/util.hpp"
#include <iomanip>
#include <iostream>
#include <bitset>

namespace stms {
    static std::random_device __seedGen;
    std::default_random_engine stmsRand(__seedGen());

    char hexDigits[16] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};

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

        uuid.timeLow = stms::intRand(0, UINT32_MAX);
        uuid.timeMid = stms::intRand(0, UINT16_MAX);
        uuid.timeHiAndVersion =
                ((unsigned) stms::intRand(0, UINT16_MAX) & (65535u ^ ((1u << 15u) | (1u << 13u) | (1u << 12u)))) |
                (1u << 14u);
        uuid.clockSeqHiAndReserved = ((unsigned) stms::intRand(0, UINT8_MAX) & (255u ^ (1u << 6u))) | (1u << 7u);
        uuid.clockSeqLow = stms::intRand(0, UINT8_MAX);
        for (unsigned char &i : uuid.node) {
            i = stms::intRand(0, UINT8_MAX);
        }

        return uuid;
    }

    std::string UUID::getStr() {
        std::stringstream stream;
        stream << std::hex << timeLow;
        stream << "-";
        stream << std::hex << timeMid;
        stream << "-";
        stream << std::hex << timeHiAndVersion;
        stream << "-";
        stream << std::hex << static_cast<unsigned>(clockSeqHiAndReserved);
        stream << std::hex << static_cast<unsigned>(clockSeqLow);
        stream << "-";
        for (uint8_t i : node) {
            stream << std::hex << static_cast<unsigned>(i);
        }

        strCache = stream.str();
        return strCache;
    }
}