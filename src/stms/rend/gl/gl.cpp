//
// Created by grant on 5/24/20.
//
#ifdef STMS_ENABLE_OPENGL

#include "stms/rend/gl/gl.hpp"

#include "stms/logging.hpp"
#include "stms/rend/gl/gl_ImGui.hpp"

namespace stms {
    bool glInitialized = false;
    GLint majorGlVersion = -1;

    void flushGlErrs(const std::string &log) {
        GLenum err = glGetError();
        while (err != GL_NO_ERROR) {
            STMS_ERROR("[* OGL ERR `{}` *]: {}", log, err);
            err = glGetError();
        }
    }
}

#endif
