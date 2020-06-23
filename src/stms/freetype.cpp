//
// Created by grant on 6/17/20.
//

#include "stms/freetype.hpp"

#include "stms/logging.hpp"

namespace stms {
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

    _stms_FTFace::~_stms_FTFace() {
        if (face != nullptr) {
            FT_Done_Face(face);
        }
    }

    _stms_FTFace::_stms_FTFace(_stms_FTFace &&rhs) noexcept {
        *this = std::move(rhs);
    }

    _stms_FTFace &_stms_FTFace::operator=(_stms_FTFace &&rhs) noexcept {
        if (rhs.face == face) {
            return *this;
        }

        FT_Done_Face(face);
        face = rhs.face;
        rhs.face = nullptr;

        return *this;
    }

    _stms_FTFace::_stms_FTFace(FTLibrary *lib, const char *filename, FT_Long index) {
        if (FT_New_Face(lib->lib, filename, index, &face)) {
            STMS_PUSH_ERROR("Failed to load font face '{}' (index {})!", filename, index);
        }
        FT_Set_Pixel_Sizes(face, 0, 50);
        newlineAdv = face->size->metrics.height >> 6;

        kern = FT_HAS_KERNING(face);
        if (!FT_HAS_HORIZONTAL(face)) {
            STMS_PUSH_WARNING("Face {} doesn't have horizontal support! This may break things!", filename);
        }
    }
}