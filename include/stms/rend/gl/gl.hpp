//
// Created by grant on 5/24/20.
//

#pragma once

#ifndef __STONEMASON_GL_GL_HPP
#define __STONEMASON_GL_GL_HPP

#ifdef STMS_ENABLE_OPENGL


#define STMS_GLC(glCall) (glCall); ::stms::flushGlErrs(#glCall);
//#define STMS_GLC(glCall) (glCall);

#define GLEW_STATIC 1
#define GLFW_INCLUDE_NONE
#include "GL/glew.h"
#include "GLFW/glfw3.h"

#include <string>

namespace stms {
    inline bool &isGlInitialized() {
        static bool val = false;
        return val;
    }

    inline GLint &getGlMajorVersion() {
        static GLint val = -1;
        return val;
    }

    void flushGlErrs(const std::string &log);

}

#endif

#endif //STONEMASON_GL_GL_HPP
