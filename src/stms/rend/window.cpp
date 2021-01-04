//
// Created by grant on 7/5/20.
//

#include "stms/rend/window.hpp"

#include "stms/logging.hpp"

namespace stms {

    static void glfwErrCb(int code, const char* description) {
        STMS_ERROR("GLFW ERROR {}: {}", code, description);
    }

    void initGlfw() {
        glfwSetErrorCallback(glfwErrCb);

        if (!glfwInit()) {
            STMS_ERROR("GLFW initialization failed! OpenGL is unusable! Expect a crash!");
        }
    }

    void quitGlfw() {
        glfwTerminate();
    }

    _stms_GenericWindow::~_stms_GenericWindow() {
        if (win != nullptr) {
            glfwDestroyWindow(win);
        }
    }
}
