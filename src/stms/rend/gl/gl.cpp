//
// Created by grant on 5/24/20.
//
#include "stms/config.hpp"
#ifdef STMS_ENABLE_OPENGL

#include "stms/rend/gl/gl.hpp"

#include "stms/logging.hpp"
#include "stms/rend/gl/imgui.hpp"

namespace stms {
    bool glInfoDumped = true;
    GLint majorGlVersion = -1;

    static void glfwErrCb(int code, const char* description) {
        STMS_ERROR("GLFW ERROR {}: {}", code, description);
    }

    void initGl() {
        glfwSetErrorCallback(glfwErrCb);

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

#endif
