/**
 * @file stms/util/compare.hpp
 * @author Grant Yang (rotartsi0482@gmail.com)
 * @brief Utilities for comparing numeric types with automatic & manual tolerances, and string edit distance.
 * @date 12/27/20
 */

#pragma once

#ifndef __STONEMASON_COMPARE_HPP
#define __STONEMASON_COMPARE_HPP
//!< Include guard

#include <limits>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string>

namespace stms {
    /**
     * @brief Class full of static functions to compare two numeric objects.
     * @tparam T The type to compare
     */
    template <typename T>
    class Cmp {
    public:

#       define STMS_CMP_MOD inline static

        /**
         * @brief Get a suitable maximum difference for two objects to be considered equal
         * @param a First object that is being compared
         * @param b Second object that is being compared
         * @return T Suitable tolerance (max difference) for `a` and `b` to be considered equal.
         */
        STMS_CMP_MOD T tolerance(const T &a, const T &b) {
            return std::numeric_limits<T>::epsilon() * std::max(std::abs(a), std::abs(b));
        }

        /**
         * @brief Get a suitable maximum difference for this object to be considered equal to another.
         * @param ref Scaling factor for epsilon.
         * @return T Suitable tolerance (max difference) for this to be considered equal to another.
         */
        STMS_CMP_MOD T tolerance(const T &ref) {
            return std::numeric_limits<T>::epsilon() * std::abs(ref);
        }

        /**
         * @brief Compare if two values are equal.
         * @param a First value
         * @param b Second value
         * @param tol Maximum difference between the two for them to be considered equal. **MUST BE POSITIVE**
         * @return true a == b
         * @return false a != b
         */
        STMS_CMP_MOD bool eq(const T &a, const T &b, const T &tol) {
            return std::abs(a - b) <= tol;
        }

        /**
         * @brief Compare two values.
         * @param a First value
         * @param b Second value
         * @param tol Maximum difference between the two for them to be considered equal. **MUST BE POSITIVE**
         * @return true a > b
         * @return false a <= b
         */
        STMS_CMP_MOD bool gt(const T &a, const T &b, const T &tol) {
            return a > b + tol;
        }

        /**
         * @brief Compare two values.
         * @param a First value
         * @param b Second value
         * @param tol Maximum difference between the two for them to be considered equal. **MUST BE POSITIVE**
         * @return true a < b
         * @return false a >= b
         */
        STMS_CMP_MOD bool lt(const T &a, const T &b, const T &tol) {
            return a < b - tol;
        }

        /**
         * @brief Compare two values.
         * @param a First value
         * @param b Second value
         * @param tol Maximum difference between the two for them to be considered equal. **MUST BE POSITIVE**
         * @return true a >= b
         * @return false a < b
         */
        STMS_CMP_MOD bool ge(const T &a, const T &b, const T &tol) {
            return a >= b - tol;
        }

        /**
         * @brief Compare two values.
         * @param a First value
         * @param b Second value
         * @param tol Maximum difference between the two for them to be considered equal. **MUST BE POSITIVE**
         * @return true a <= b
         * @return false a > b
         */
        STMS_CMP_MOD bool le(const T &a, const T &b, const T &tol) {
            return a <= b + tol;
        }


#       define STMS_AUTO_TOLERANCE(exp) STMS_CMP_MOD bool exp(const T &a, const T &b) { \
            return exp(a, b, Cmp<T>::tolerance(a, b));                             \
        }

        STMS_AUTO_TOLERANCE(eq); //!< Overload of `Cmp<T>::eq` where `tol` is automatically determined.
        STMS_AUTO_TOLERANCE(gt); //!< Overload of `Cmp<T>::gt` where `tol` is automatically determined.
        STMS_AUTO_TOLERANCE(lt); //!< Overload of `Cmp<T>::lt` where `tol` is automatically determined.
        STMS_AUTO_TOLERANCE(ge); //!< Overload of `Cmp<T>::ge` where `tol` is automatically determined.
        STMS_AUTO_TOLERANCE(le); //!< Overload of `Cmp<T>::le` where `tol` is automatically determined.

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
