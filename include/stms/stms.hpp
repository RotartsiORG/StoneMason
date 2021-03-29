/**
 * @file stms/stms.hpp
 * @brief Provides common STMS functionality
 * @author Grant Yang (rotartsi0482@gmail.com)
 * @date 4/21/20
 */

#pragma once

#ifndef __STONEMASON_STMS_HPP
#define __STONEMASON_STMS_HPP
//!< Include guard

#include <string>
#include <random>

#include <cstdlib>
#include <cmath>

namespace stms {
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
}

#endif //__STONEMASON_UTIL_HPP
