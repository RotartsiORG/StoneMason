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

    class GenericWindow {
    protected:
        GLFWwindow *win = nullptr;

        GenericWindow() = default;
    public:
        virtual ~GenericWindow();

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
    };
}

#endif //STONEMASON_WINDOW_HPP
