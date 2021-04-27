//
// Created by grant on 12/27/20.
//

#include "stms/util/util.hpp"

#include <openssl/rand.h>
#include "openssl/ssl.h"

#include "stms/logging.hpp"

#include <random>
#include <algorithm>
#include <fstream>

namespace stms {
    int intRand(int min, int max) {
        std::uniform_int_distribution<> dist(min, max);
        return dist(stmsRand());
    }

    float floatRand(float min, float max) {
        std::uniform_real_distribution<> dist(min, max);
        return dist(stmsRand());
    }


    std::string toHex(unsigned long long in, uint8_t places) {
        std::string ret;
        ret.reserve(places);

        uint8_t i = 0;
        uint8_t lastAdded = in % 16;

        // Even if i > places, it will keep going until we get a 0 so that we retain 100% of the data
        // Is this behaviour good?
        while (lastAdded != 0 || i < places) {
            ret += hexChars[lastAdded];
            lastAdded = (in / static_cast<unsigned long long>(std::pow(16, ++i))) % 16;
        }

        std::reverse(ret.begin(), ret.end());
        return ret;
    }

    std::vector<uint8_t> readFile(const std::string &filename) {
        // Open in binary mode, this might cause issues with line endings but fuck windows
        std::ifstream in(filename, std::ios::ate | std::ios::binary);
        if (!in.is_open()) {
            STMS_WARN("Unable to open {}!", filename);
            if (exceptionLevel > 0) {
                throw std::runtime_error(fmt::format("Unable to open '{}'", filename));
            }
            return {};
        }

        in.unsetf(std::ios::skipws); // Don't skip whitespace


        std::vector<uint8_t> ret;
        ret.reserve(in.tellg());
        in.seekg(0, std::ios::beg);
        
        ret.insert(ret.begin(),
                std::istream_iterator<uint8_t>(in),
                std::istream_iterator<uint8_t>());
        return ret;
    }

    size_t boostHashCombine(size_t lhs, size_t rhs) {
        return lhs ^ (rhs + 0x9e3779b9 + (lhs << 6UL) + (lhs >> 2UL));
    }

    int sslRand(unsigned char *buf, int size) {
        return RAND_bytes(buf, size);
    }

    std::default_random_engine &stmsRand() {
        static std::random_device seedGen = std::random_device();
        static std::default_random_engine ret(seedGen());
        return ret;
    }

}
