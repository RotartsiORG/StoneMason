//
// Created by grant on 7/5/20.
//

#pragma once

#ifndef __STONEMASON_WINDOW_HPP
#define __STONEMASON_WINDOW_HPP

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

namespace stms {
    void initGlfw();

    void quitGlfw();

    inline void pollEvents() {
        glfwPollEvents();
    };

    class _stms_GenericWindow {
    protected:
        _stms_GenericWindow() = default;

    public:
        GLFWwindow *win = nullptr;

        virtual ~_stms_GenericWindow();

        inline bool shouldClose() {
            return glfwWindowShouldClose(win);
        }

        inline glm::ivec2 getSize() {
            glm::ivec2 ret;
            glfwGetWindowSize(win, &ret.x, &ret.y);
            return ret;
        }

        inline GLFWwindow *getRawPtr() {
            return win;
        }

        inline void focus() { glfwFocusWindow(win); }

        inline void requestAttention() { glfwRequestWindowAttention(win); }

        inline void hide() { glfwHideWindow(win); }

        inline void show() { glfwShowWindow(win); }

        inline void maximize() { glfwMaximizeWindow(win); }

        inline void iconify() { glfwIconifyWindow(win); }

        inline void restore() { glfwRestoreWindow(win); }

        inline void setTitle(const char *title) { glfwSetWindowTitle(win, title); }

        inline void setPos(glm::ivec2 pos) { glfwSetWindowPos(win, pos.x, pos.y); }

    };
}

#endif //STONEMASON_WINDOW_HPP
