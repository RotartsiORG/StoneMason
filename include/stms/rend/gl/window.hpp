//
// Created by grant on 5/24/20.
//

#pragma once

#ifndef __STONEMASON_GL_WINDOW_HPP
#define __STONEMASON_GL_WINDOW_HPP

#include "gl.hpp"

#include <unordered_map>

namespace stms {

    class GLWindow {
    private:
        GLFWwindow *win = nullptr;
    public:
        GLWindow() = default;
        GLWindow(int width, int height, const char *title="StoneMason window");
        virtual ~GLWindow();

        inline bool shouldClose() {
            return glfwWindowShouldClose(win);
        }

        inline void flip() {
            glfwSwapBuffers(win);
        }

        inline void lazyFlip() {
            glfwPollEvents();
            glfwSwapBuffers(win);
            STMS_GLC(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)); // Stencil buffer is omitted.
        }

        GLWindow(const GLWindow &rhs) = delete;
        GLWindow &operator=(const GLWindow &rhs) = delete;

        inline GLFWwindow *getRawPtr() {
            return win;
        }

        GLWindow(GLWindow &&rhs) noexcept;
        GLWindow &operator=(GLWindow &&rhs) noexcept;
    };

    extern void(*pollEvents)();
}


#endif //__STONEMASON_GL_WINDOW_HPP
