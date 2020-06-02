//
// Created by grant on 5/24/20.
//

#include "stms/rend/gl/gl.hpp"

#include "stms/logging.hpp"

namespace stms::rend {
    bool glInfoDumped = true;
    GLint majorGlVersion = -1;

    void initGl() {
        if (!glfwInit()) {
            STMS_FATAL("GLFW initialization failed! OpenGL is unusable! Aborting!");
            std::terminate();
        }
    }
}
