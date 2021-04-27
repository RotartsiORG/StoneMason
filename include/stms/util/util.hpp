/**
 * @file stms/util/util.hpp
 * @brief Provides some general utilities like RNG, hash combine, decimal to hex, & easily reading files.
 * @author Grant Yang (rotartsi0482@gmail.com)
 * @date 12/27/20
 */

#pragma once

#ifndef __STONEMASON_UTIL_HPP
#define __STONEMASON_UTIL_HPP

#include <random>

namespace stms {

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
     * @brief Boost's hash combine function poached from
     *        https://www.boost.org/doc/libs/1_73_0/boost/container_hash/hash.hpp Not sure if this function
     *        is very good, but it seems to work well enough.
     * @param lhs First 64-bit hash you want to combine
     * @param rhs Second 64-bit hash you want to combine
     * @return Combined 64-bit hash.
     */
    size_t boostHashCombine(size_t lhs, size_t rhs);

    constexpr char hexChars[] = "0123456789abcdef"; //!< List of characters for hex, used for `toHex`

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

    /// Returns the default c++ non-cryptographic random number generator.
    std::default_random_engine &stmsRand();

    /**
     * @brief Read the contents of a file
     * @throw If `stms::exceptionLevel` > 0, an `std::runtime_error` is thrown if `filename` cannot be read.
     * @param filename File to read
     * @return Contents of file as vector of unsigned bytes.
     */
    std::vector<uint8_t> readFile(const std::string &filename);

    /**
     * @brief Simple wrapper around OpenSSL's `RAND_bytes`, a cryptographically secure RNG
     * @param buf Where to write the random data.
     * @param size How many bytes of random data to write.
     * @return Bytes of random data successfully written.
     */
    inline int sslRand(unsigned char * buf, int size);
}

#endif //STMS_STATIC_UTIL_HPP
