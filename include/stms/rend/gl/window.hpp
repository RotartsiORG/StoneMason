//
// Created by grant on 5/24/20.
//

#pragma once

#ifndef __STONEMASON_GL_WINDOW_HPP
#define __STONEMASON_GL_WINDOW_HPP

#define GLEW_STATIC
#include "GL/glew.h"
#include "GLFW/glfw3.h"

namespace stms::rend {
    class GLWindow {
    private:
        GLFWwindow *win;
    public:
        GLWindow() = default;
        GLWindow(int width, int height, const char *title="StoneMason window");
        virtual ~GLWindow();

        GLWindow(const GLWindow &rhs) = delete;
        GLWindow &operator=(const GLWindow &rhs) = delete;

        GLWindow(GLWindow &&rhs) noexcept;
        GLWindow &operator=(GLWindow &&rhs) noexcept;
    };
}


#endif //__STONEMASON_GL_WINDOW_HPP
