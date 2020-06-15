//
// Created by grant on 5/24/20.
//

#include "stms/rend/gl/window.hpp"

#include "stms/rend/gl/gl.hpp"
#include "stms/logging.hpp"
#include "stms/config.hpp"

namespace stms::rend {

    void(*pollEvents)() = &glfwPollEvents;
    void(*quitGlfw)() = &glfwTerminate;

    GLWindow::GLWindow(int width, int height, const char *title) {
        win = glfwCreateWindow(width, height, title, nullptr, nullptr);
        if (!win) {
            STMS_PUSH_ERROR("GLFW window creation failed! This window is unusable!");
        }

        glfwMakeContextCurrent(win);

        glewExperimental = enableExperimentalGlew;
        GLenum err = glewInit();
        if (err != GLEW_OK) {
            STMS_PUSH_ERROR("GLEW failed to load OpenGL implementation! OpenGL for this window is unusable! Expect a crash!");
        }

        if (majorGlVersion == -1) {
            glGetIntegerv(GL_MAJOR_VERSION, &majorGlVersion);
        }

        if (glInfoDumped) {
            glInfoDumped = false;
            STMS_INFO("Initialized OpenGL {} with GLEW {}", glGetString(GL_VERSION), glewGetString(GLEW_VERSION));
            STMS_INFO("OpenGL vendor is {} with renderer {}", glGetString(GL_VENDOR), glGetString(GL_RENDERER));
            STMS_INFO("GLSL version is {} and got major version {}", glGetString(GL_SHADING_LANGUAGE_VERSION), majorGlVersion);
            STMS_INFO("Enabled OpenGL exts: '{}'", glGetString(GL_EXTENSIONS));
        }

        if (majorGlVersion < 2) {
            STMS_PUSH_WARNING("Your OpenGL version is outdated and unsupported! Expect crashes and/or bugs!");
        }
    }

    GLWindow::~GLWindow() {
        glfwDestroyWindow(win);
    }
}
