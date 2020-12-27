//
// Created by grant on 12/27/20.
//

#include "stms/util/compare.hpp"

namespace stms {

    // Modified version of https://leetcode.com/problems/edit-distance/discuss/987206/C++-DP-O(min(m-n))-8ms-Memory-Beats-99.94
    unsigned editDistance(const std::string &word1, const std::string &word2) {

        // Determine which string is shorter.
        const std::string *shortPtr, *longPtr;
        if (word1.length() < word2.length()) {
            shortPtr = &word1;
            longPtr = &word2;
        } else {
            shortPtr = &word2;
            longPtr = &word1;
        }

        // O(min(m, n))
        auto *cache = new unsigned[shortPtr->length() + 1];
        for (unsigned i = 0; i < shortPtr->length() + 1; i++) { cache[i] = i; }

        // tmp1: storing the most recently overridden element of cache
        // tmp2: storing the currently calculated value
        unsigned tmp1, tmp2;

        for (unsigned i = 1; i < longPtr->length() + 1; i++) {
            tmp1 = cache[0];
            cache[0] = i;
            for (unsigned j = 1; j < shortPtr->length() + 1; j++) {
                if (longPtr->at(i - 1) == shortPtr->at(j - 1)) {
                    // Two strings have the same final character for this sub-problem
                    tmp2 = tmp1;
                    tmp1 = cache[j];
                    cache[j] = tmp2;
                } else {
                    tmp2 = std::min(std::min(tmp1, cache[j - 1]), cache[j]) + 1;
                    tmp1 = cache[j];
                    cache[j] = tmp2;
                }
            }
        }

        unsigned ret = cache[shortPtr->length()];
        delete[] cache;
        return ret;
    }
}

