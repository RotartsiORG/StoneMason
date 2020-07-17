//
// Created by grant on 5/24/20.
//

#pragma once

#ifndef __STONEMASON_GL_WINDOW_HPP
#define __STONEMASON_GL_WINDOW_HPP

#include "stms/config.hpp"
#ifdef STMS_ENABLE_OPENGL

#include "gl.hpp"

#include "stms/rend/gl/gl_ImGui.hpp"

#include "stms/rend/window.hpp"
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

    class GLWindow : public GenericWindow {
    public:
        GLWindow(int width, int height, const char *title="StoneMason window");
        ~GLWindow() override;

        inline void flip() {
            glfwSwapBuffers(win);
        }

        GLWindow(const GLWindow &rhs) = delete;
        GLWindow &operator=(const GLWindow &rhs) = delete;

        // TODO: implement
        GLWindow(GLWindow &&rhs) noexcept;
        GLWindow &operator=(GLWindow &&rhs) noexcept;
    };

    void newImGuiFrame();

    void renderImGui();

    // some direct pointers to raw opengl calls
    // TODO: Depth & stencil testing configuration
    inline void enableGl(unsigned feat) {
        STMS_GLC(glEnable(feat));
    }
    inline void disableGl(unsigned feat) {
        STMS_GLC(glDisable(feat));
    }
    inline void clearGl(unsigned bufferBits) {
        STMS_GLC(glClear(bufferBits));
    }

    inline void viewportGl(int x, int y, int w, int h) {
        STMS_GLC(glViewport(x, y, w, h));
    }
}

#endif

#endif //__STONEMASON_GL_WINDOW_HPP
