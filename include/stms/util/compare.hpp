//
// Created by grant on 12/27/20.
//

#pragma once

#ifndef __STONEMASON_COMPARE_HPP
#define __STONEMASON_COMPARE_HPP

#include <limits>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string>

namespace stms {
    template <typename T>
    class Cmp {
    public:

#       define STMS_CMP_MOD inline static

        STMS_CMP_MOD T tolerance(const T &a, const T &b) {
            return std::numeric_limits<T>::epsilon() * std::max(std::abs(a), std::abs(b));
        }

        STMS_CMP_MOD T tolerance(const T &ref) {
            return std::numeric_limits<T>::epsilon() * std::abs(ref);
        }

        // The following functions all assume that `tol` is positive

        STMS_CMP_MOD bool eq(const T &a, const T &b, const T &tol) { // define a `ne()` not equal inverse? pointless.
            return std::abs(a - b) <= tol;
        }

        STMS_CMP_MOD bool gt(const T &a, const T &b, const T &tol) {
            return a > b + tol;
        }

        STMS_CMP_MOD bool lt(const T &a, const T &b, const T &tol) {
            return a < b - tol;
        }

        STMS_CMP_MOD bool ge(const T &a, const T &b, const T &tol) {
            return a >= b - tol;
        }

        STMS_CMP_MOD bool le(const T &a, const T &b, const T &tol) {
            return a <= b + tol;
        }


#       define STMS_AUTO_TOLERANCE(exp) STMS_CMP_MOD bool exp(const T &a, const T &b) { \
            return exp(a, b, Cmp<T>::tolerance(a, b));                             \
        }

        STMS_AUTO_TOLERANCE(eq);
        STMS_AUTO_TOLERANCE(gt);
        STMS_AUTO_TOLERANCE(lt);
        STMS_AUTO_TOLERANCE(ge);
        STMS_AUTO_TOLERANCE(le);

#       undef STMS_AUTO_TOLERANCE
#       undef STMS_CMP_MOD

    };

    /**
     * @brief Find the levenshtein edit distance between two strings.
     *        From https://leetcode.com/problems/edit-distance/discuss/987206/C++-DP-O(min(m-n))-8ms-Memory-Beats-99.94
     * @param word1 First string
     * @param word2 Second string
     * @return Edit distance. Possible operations: Insert single character, remove single character, replace character.
     */
    unsigned editDistance(const std::string &word1, const std::string &word2);
}


#endif //__STONEMASON_COMPARE_HPP
