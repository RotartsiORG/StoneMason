//
// Created by grant on 6/17/20.
//

#include "stms/rend/gl/freetype.hpp"

#include "stms/logging.hpp"

namespace stms::rend {
    FTFace::~FTFace() {
        FT_Done_Face(face);
    }

    FTFace::FTFace(FTFace &&rhs) noexcept {
        *this = std::move(rhs);
    }

    FTFace &FTFace::operator=(FTFace &&rhs) noexcept {
        if (rhs.face == face) {
            return *this;
        }

        FT_Done_Face(face);
        face = rhs.face;
        rhs.face = nullptr;

        return *this;
    }

    FTLibrary::FTLibrary() {
        FT_Init_FreeType(&lib);
    }

    FTLibrary::~FTLibrary() {
        FT_Done_FreeType(lib);
        lib = nullptr;
    }

    FTLibrary::FTLibrary(FTLibrary &&rhs) noexcept {
        *this = std::move(rhs);
    }

    FTLibrary &FTLibrary::operator=(FTLibrary &&rhs) noexcept {
        if (rhs.lib == lib) {
            return *this;
        }

        FT_Done_FreeType(lib);
        lib = rhs.lib;
        rhs.lib = nullptr;
        return *this;
    }

    FTFace FTLibrary::newFace(const char *filename, FT_Long index) {
        auto ret = FTFace();
        if (FT_New_Face(lib, filename, index, &ret.face)) {
            STMS_PUSH_ERROR("Failed to load font face '{}' (index {})!", filename, index);
        }
        return ret;
    }
}