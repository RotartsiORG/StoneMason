/**
 * @file stms/stms.hpp
 * @brief Provides common STMS functionality
 * Created by grant on 4/21/20.
 */

#pragma once

#ifndef __STONEMASON_UTIL_HPP
#define __STONEMASON_UTIL_HPP
//!< Include guard

#include <string>
#include <random>

#include <cstdlib>
#include <cmath>

#include <openssl/rand.h>
#include "openssl/ssl.h"

namespace stms {

    constexpr char hexChars[] = "0123456789abcdef"; //!< List of characters for hex, used for `toHex`

    /// Returns the default c++ non-cryptographic random number generator.
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

    /// UUID type, or version. In the future, other types of UUIDs may be supported (in addition to UUIDv4)
    enum class UUIDType {
        eUninitialized, //!< Used for UUIDs where all 128 bits are set to 0
        eUuid4 //!< Used for UUIDv4s, where 124 bits are randomly set. See RFC 4122 for more info
    };

    /**
     * @brief Simple wrapper around OpenSSL's `RAND_bytes`, a cryptographically secure RNG
     * @param buf Where to write the random data.
     * @param size How many bytes of random data to write.
     * @return Bytes of random data successfully written.
     */
    inline int sslRand(unsigned char * buf, int size) {
        return RAND_bytes(buf, size);
    }

    /**
     * @brief Convert an integer into hexadecimal. Simple base10 to base16 conversion
     * @param in Number to convert to hex
     * @param places Minimum number of digits to have. Return string will be padded with leading 0s if
     *               `in` in hex doesn't reach `places` digits. However, if `in` in hex is greater than
     *               `places` digits, the full hex representation will be returned (i.e. digits will never
     *               be truncated).
     * @return Hex string, with no leading `0x`.
     */
    std::string toHex(unsigned long long in, uint8_t places = 0);

    /// 128-bit Universally Unique Identifier. See RFC 4122
    struct UUID {
    public:
        uint32_t timeLow{}; //!< Low field of timestamp. Called `time_low` in the RFC
        uint16_t timeMid{}; //!< Mid field of timestamp. Called `time_mid` in the RFC
        uint16_t timeHiAndVersion{}; //!< High field of timestamp multiplex with version number. `time_hi_and_version`
        uint8_t clockSeqHiAndReserved{}; //!< High field of clock sequence multiplexed with the variant.
        uint8_t clockSeqLow{}; //!< Low field of the clock sequence. `clock_seq_low` in RFC
        uint8_t node[6]{}; //!< Spatially unique node identifier. Unsigned 48-bit int. Called `node` in the RFC.

        /**
         * @brief Convert this UUID to a human-readable hex string, as specified in RFC 4122
         * @return 36-character long string.
         */
        [[nodiscard]] std::string buildStr() const;

        UUID() = default; //!< Initializes UUID to all 128 bits being 0s

        /**
         * @brief Normal constructor for UUIDs
         * @param type Type of UUID to initialize.
         *             Use `eUninitialized` for all 0s
         *             Use `eUuid4` for random 124 bits. See RFC 4122 for UUIDv4. Equivalent to `genUUID4`
         */
        explicit UUID(UUIDType type);

        UUID(const UUID &rhs); //!< Cpy c'tor

        UUID &operator=(const UUID &rhs); //!< cpy op

        bool operator==(const UUID &rhs) const; //!< Equality op

        bool operator!=(const UUID &rhs) const; //!< Ineq op
    };

    /**
     * @brief Generate a UUIDv4. See RFC 4122. Equivalent to `UUID(eUuid4)`
     * @return Generated UUID
     */
    UUID genUUID4();

    /**
     * @brief Get a random integer from the non-crypto c++ RNG (uniform distrib)
     * @param min Lower bound of random integer, inclusive
     * @param max Upper bound of random integer, inclusive
     * @return Random integer between [min, max]
     */
    int intRand(int min, int max);

    /**
     * @brief Get a random real number from the non-crypto c++ RNG (uniform distrib)
     * @param min Lower bound of random integer, inclusive
     * @param max Upper bound of random integer, inclusive
     * @return Random real number between [min, max]
     */
    float floatRand(float min, float max);

    /**
     * @brief Implementation detail, don't touch. This class runs the initialization routine for STMS in its constructor
     */
    class _stms_STMSInitializer {

    public:
        static bool hasRun; //!< Static flag for if STMS has already been initialized

        uint8_t specialValue; //!< Special non-static value to be written to stdout by `initAll` to prevent `_stms_initializer` from being optimized out.

        _stms_STMSInitializer() noexcept; //!< Constructor that runs STMS's initialization routine

        ~_stms_STMSInitializer() noexcept; //!< Destructor that runs STMS's quit routine, so you don't have to call a function to quit STMS

        _stms_STMSInitializer(_stms_STMSInitializer &rhs) = delete; //!< Duh

        _stms_STMSInitializer &operator=(_stms_STMSInitializer &rhs) = delete; //!< duh
    };

    /// Implementation detail, don't touch. See `initAll` and `_stms_STMSInitializer`. This object's c'tor inits STMS.
    extern _stms_STMSInitializer _stms_initializer;

    /**
     * @brief Initialize all components of STMS. Works by preventing `_stms_initializer` from being optimized out,
     *        so it doesn't really matter where you call this; STMS initialization will always take place during
     *        the static variable initialization phase.
     */
    void initAll();

    /**
     * @brief Boost's hash combine function poached from
     *        https://www.boost.org/doc/libs/1_73_0/boost/container_hash/hash.hpp Not sure if this function
     *        is very good, but it seems to work well enough.
     * @param lhs First 64-bit hash you want to combine
     * @param rhs Second 64-bit hash you want to combine
     * @return Combined 64-bit hash.
     */
    size_t boostHashCombine(size_t lhs, size_t rhs);


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
}



/// Namespace full of the nastiest sexually transmitted diseases (JK it's what you think it is))
namespace std {

    /// Hash function for `stms::UUID`s so that they can be used in maps and stuff.
    template<> struct hash<stms::UUID> {

        /**
         * @brief Implementation of the hash function. This simply calls `stms::boostHashCombine`
         *        on the lower and upper 64 bits of the UUID
         * @param s UUID to hash
         * @return Combined lower and upper halves of the UUID
         */
        std::size_t operator()(stms::UUID const& s) const noexcept {
            auto *lowerHalf = reinterpret_cast<const uint64_t *>(&s);
            return stms::boostHashCombine(*lowerHalf, *(lowerHalf + 1));
        }
    };
}


#endif //__STONEMASON_UTIL_HPP
