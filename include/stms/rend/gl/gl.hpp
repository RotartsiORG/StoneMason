//
// Created by grant on 5/24/20.
//

#pragma once

#ifndef __STONEMASON_GL_GL_HPP
#define __STONEMASON_GL_GL_HPP

#include "stms/config.hpp"
#ifdef STMS_ENABLE_OPENGL


#define STMS_GLC(glCall) (glCall); ::stms::flushGlErrs(#glCall);
//#define STMS_GLC(glCall) (glCall);

#define GLEW_STATIC 1
#define GLFW_INCLUDE_NONE
#include "GL/glew.h"
#include "GLFW/glfw3.h"

#include <string>

namespace stms {
    extern bool glInitialized;
    extern GLint majorGlVersion;

    void flushGlErrs(const std::string &log);

}

#endif

#endif //STONEMASON_GL_GL_HPP
