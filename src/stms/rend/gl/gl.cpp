//
// Created by grant on 5/24/20.
//

#include "stms/rend/gl/gl.hpp"

#include "stms/logging.hpp"

namespace stms {
    bool glInfoDumped = true;
    GLint majorGlVersion = -1;

    void initGl() {
        if (!glfwInit()) {
            STMS_ERROR("GLFW initialization failed! OpenGL is unusable! Expect a crash!");
        }
    }

    void flushGlErrs(const std::string &log) {
        GLenum err = glGetError();
        while (err != GL_NO_ERROR) {
            STMS_ERROR("[* OGL ERR `{}` *]: {}", log, err);
            err = glGetError();
        }
    }

    void quitGl() {
        glfwTerminate();
    }
}
