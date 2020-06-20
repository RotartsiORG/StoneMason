//
// Created by grant on 6/17/20.
//

#pragma once

#ifndef __STONEMASON_GL_FREETYPE_HPP
#define __STONEMASON_GL_FREETYPE_HPP

#include "ft2build.h"
#include FT_FREETYPE_H

#include "glm/glm.hpp"

#include <string>
#include <unordered_map>

namespace stms::rend {

    class FTLibrary {
    private:
        FT_Library lib = nullptr;

        friend class _stms_FTFace;
    public:
        FTLibrary();
        virtual ~FTLibrary();

        FTLibrary(const FTLibrary &rhs) = delete;
        FTLibrary &operator=(const FTLibrary &rhs) = delete;

        FTLibrary(FTLibrary &&rhs) noexcept;
        FTLibrary &operator=(FTLibrary &&rhs) noexcept;
    };

    class _stms_FTFace {
    protected:
        FT_Face face = nullptr;

    public:
        _stms_FTFace(FTLibrary *lib, const char *filename, FT_Long index = 0);
        virtual ~_stms_FTFace();

        _stms_FTFace(const _stms_FTFace &rhs) = delete;
        _stms_FTFace &operator=(const _stms_FTFace &rhs) = delete;

        _stms_FTFace(_stms_FTFace &&rhs) noexcept;
        _stms_FTFace &operator=(_stms_FTFace &&rhs) noexcept;

        inline void setSize(FT_UInt height, FT_UInt width = 0) {
            FT_Set_Pixel_Sizes(face, width, height);
        }
    };

    inline FTLibrary &defaultFtLib() {
        static FTLibrary defLib = FTLibrary();
        return defLib;
    }
}


#endif //__STONEMASON_GL_FREETYPE_HPP
