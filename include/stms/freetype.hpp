/**
 * @file stms/freetype.hpp
 * @brief STMS's C++ wrapper around freetype. Backend-agnostic text rendering core.
 * Created by grant on 6/17/20.
 */

#pragma once

#ifndef __STONEMASON_GL_FREETYPE_HPP
#define __STONEMASON_GL_FREETYPE_HPP
//!< Include guard

#include "ft2build.h"
#include FT_FREETYPE_H

#include "glm/glm.hpp"

#include <string>
#include <unordered_map>

namespace stms {

    /// 32-bit string. Wide enough to contain any unicode character.
    typedef std::basic_string<char32_t> U32String;

    /// Freetype library instance, wrapper around `FT_Library`. Use `defaultFtLib()` instead of constructing your own.
    class FTLibrary {
    private:
        FT_Library lib = nullptr; //!< Internal FT_Library. Implementation detail, no touchy

        friend class _stms_FTFace;
    public:
        FTLibrary(); //!< duh
        virtual ~FTLibrary(); //!< duh

        FTLibrary(const FTLibrary &rhs) = delete; //!< duh
        FTLibrary &operator=(const FTLibrary &rhs) = delete; //!< duh

        FTLibrary(FTLibrary &&rhs) noexcept; //!< duh
        FTLibrary &operator=(FTLibrary &&rhs) noexcept; //!< duh
    };

    /**
     * @brief Render backend-agnostic wrapper around `FT_Face`. Don't use directly, as this is an implementation detail.
     *        Represents a font and can be loaded from a .ttf file. Don't use this. Use `GLFTFace` for OpenGL.
     */
    class _stms_FTFace {
    protected:
        FT_Face face = nullptr; //!< Raw FT_Face wrapped around. Implementation detail.
        bool kern = false; //!< Bool flag for if this typeface has kerning info
        float newlineAdv = 50; //!< How many units we should advance every newline (`\n`)
        float spacing = 1.0f; //!< Multiplier for `newlineAdv`. 2.0 is double-spaced, 1.0 is single spaced, etc.
        int tabSize = 4; //!< How many spaces equals 1 tab. Tabs are processed normally and are NOT fixed width.

    public:
        /**
         * @brief Construct a new font face
         * @param lib FTLibrary object to use. You usually want `defaultFtLib()`
         * @param filename Path to the .ttf file you want to load
         * @param index Font variation index, for stuff like bold and italics. Defautls to 0
         */
        _stms_FTFace(FTLibrary *lib, const char *filename, FT_Long index = 0);
        virtual ~_stms_FTFace(); //!< duh

        _stms_FTFace(const _stms_FTFace &rhs) = delete; //!< duh
        _stms_FTFace &operator=(const _stms_FTFace &rhs) = delete; //!< duh

        _stms_FTFace(_stms_FTFace &&rhs) noexcept; //!< duh
        _stms_FTFace &operator=(_stms_FTFace &&rhs) noexcept; //!< duh

        /**
         * @brief Set the font size
         * @param height Height of the font. This is usually the metric you see in word processors.
         * @param width Width of the font. If set to 0, width will be determined based on `height`, which is usually
         *              what you want.
         */
        inline void setSize(FT_UInt height, FT_UInt width = 0) {
            FT_Set_Pixel_Sizes(face, width, height);
            newlineAdv = face->size->metrics.height >> 6;
        }

        inline void setSpacing(float newSpacing) {
            spacing = newSpacing;
        }

        inline void setTabSize(int spaces) {
            tabSize = spaces;
        }
    };

    inline FTLibrary &defaultFtLib() {
        static FTLibrary defLib = FTLibrary();
        return defLib;
    }
}


#endif //__STONEMASON_GL_FREETYPE_HPP
