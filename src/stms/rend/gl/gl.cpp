//
// Created by grant on 5/24/20.
//

#include "stms/rend/gl/gl.hpp"

#include "stms/logging.hpp"

#define GLEW_STATIC
#include "GL/glew.h"
#include "GLFW/glfw3.h"

namespace stms::rend {
    bool glInfoDumped = true;

    void initGl() {
        if (!glfwInit()) {
            STMS_FATAL("GLFW initialization failed! OpenGL is unusable! Aborting!");
            std::terminate();
        }
    }
}
