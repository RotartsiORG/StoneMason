//
// Created by grant on 5/24/20.
//

#include "stms/rend/gl/window.hpp"

#include "stms/rend/gl/gl.hpp"
#include "stms/logging.hpp"

namespace stms::gl {
    Window::Window(int width, int height, const char *title) {
        win = glfwCreateWindow(width, height, title, nullptr, nullptr);
        if (!win) {
            STMS_FATAL("GLFW window creation failed! This window is unusable!");
        }

        glfwMakeContextCurrent(win);

        GLenum err = glewInit();
        if (err != GLEW_OK) {
            STMS_FATAL("GLEW failed to load OpenGL implementation! OpenGL for this window is unusable!");
        }

        if (glInfoDumped) {
            glInfoDumped = false;
            STMS_INFO("Initialized OpenGL {} with GLEW {}", glGetString(GL_VERSION), glewGetString(GLEW_VERSION));
            STMS_INFO("OpenGL vendor is {} with renderer {}", glGetString(GL_VENDOR), glGetString(GL_RENDERER));
            STMS_INFO("GLSL version is {} and got major version {}", glGetString(GL_SHADING_LANGUAGE_VERSION));
            STMS_INFO("Enabled OpenGL exts: '{}'", glGetString(GL_EXTENSIONS));
        }
    }

    Window::~Window() {
        glfwDestroyWindow(win);
    }
}
