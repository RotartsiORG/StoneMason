//
// Created by grant on 6/17/20.
//

#pragma once

#ifndef __STONEMASON_GL_FREETYPE_HPP
#define __STONEMASON_GL_FREETYPE_HPP

#include "ft2build.h"
#include FT_FREETYPE_H

namespace stms::rend {
    class FTFace {
    private:
        FT_Face face = nullptr;

        friend class FTLibrary;

    public:
        FTFace() = default;
        virtual ~FTFace();

        FTFace(const FTFace &rhs) = delete;
        FTFace &operator=(const FTFace &rhs) = delete;

        FTFace(FTFace &&rhs) noexcept;
        FTFace &operator=(FTFace &&rhs) noexcept;
    };

    class FTLibrary {
    private:
        FT_Library lib = nullptr;

    public:
        FTLibrary();
        virtual ~FTLibrary();

        FTLibrary(const FTLibrary &rhs) = delete;
        FTLibrary &operator=(const FTLibrary &rhs) = delete;

        FTLibrary(FTLibrary &&rhs) noexcept;
        FTLibrary &operator=(FTLibrary &&rhs) noexcept;

        /**
         * Load a new `FTFace` object from a specified filename.
         * @param filename The font file to load
         * @param index See freetype docs for `face_index` attrib in https://www.freetype.org/freetype2/docs/reference/ft2-base_interface.html#ft_open_face
         * @return `FTFace` object loaded from the specified filename and face index.
         */
        FTFace newFace(const char *filename, FT_Long index = 0);
    };
}


#endif //__STONEMASON_GL_FREETYPE_HPP
