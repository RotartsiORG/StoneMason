//
// Created by grant on 5/24/20.
//

#pragma once

#ifndef __STONEMASON_GL_WINDOW_HPP
#define __STONEMASON_GL_WINDOW_HPP

#include "gl.hpp"

#include <glm/glm.hpp>

#include "stms/rend/gl/imgui.hpp"

#include <unordered_map>

namespace stms {

    enum GLClearBits {
        eColor = GL_COLOR_BUFFER_BIT,
        eDepth = GL_DEPTH_BUFFER_BIT,
        eStencil = GL_STENCIL_BUFFER_BIT
    };

    enum GLFeatures {
        eCulling = GL_CULL_FACE,
        eBlending = GL_BLEND,
        eDepthTest = GL_DEPTH_TEST,
        eStencilTest = GL_STENCIL_TEST
    };

    class GLWindow {
    private:
        GLFWwindow *win = nullptr;
    public:
        GLWindow(int width, int height, const char *title="StoneMason window");
        virtual ~GLWindow();

        inline bool shouldClose() {
            return glfwWindowShouldClose(win);
        }

        inline void flip() {
            glfwSwapBuffers(win);
        }

        inline glm::ivec2 getSize() {
            glm::ivec2 ret;
            glfwGetWindowSize(win, &ret.x, &ret.y);
            return ret;
        }

        GLWindow(const GLWindow &rhs) = delete;
        GLWindow &operator=(const GLWindow &rhs) = delete;

        inline GLFWwindow *getRawPtr() {
            return win;
        }

        GLWindow(GLWindow &&rhs) noexcept;
        GLWindow &operator=(GLWindow &&rhs) noexcept;
    };

    void newImGuiFrame();

    void renderImGui();

    // some direct pointers to raw opengl calls
    // TODO: Depth & stencil testing configuration
    extern void(*enableGl)(unsigned);
    extern void(*disableGl)(unsigned);
    extern void(*clearGl)(unsigned);
    extern void(*viewportGl)(int, int, int, int);
    extern void(*pollEvents)();
}


#endif //__STONEMASON_GL_WINDOW_HPP
