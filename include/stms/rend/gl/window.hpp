//
// Created by grant on 5/24/20.
//

#pragma once

#ifndef __STONEMASON_GL_WINDOW_HPP
#define __STONEMASON_GL_WINDOW_HPP

#include "GL/glew.h"
#include "GLFW/glfw3.h"

namespace stms::gl {
    class Window {
    private:
        GLFWwindow *win;
    public:
        Window() = default;
        Window(int width, int height, const char *title="StoneMason window");
        virtual ~Window();

        Window(const Window &rhs) = delete;
        Window &operator=(const Window &rhs) = delete;

        Window(Window &&rhs) noexcept;
        Window &operator=(Window &&rhs) noexcept;
    };
}


#endif //__STONEMASON_GL_WINDOW_HPP
