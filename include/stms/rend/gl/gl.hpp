//
// Created by grant on 5/24/20.
//

#pragma once

#ifndef __STONEMASON_GL_GL_HPP
#define __STONEMASON_GL_GL_HPP

#define GLEW_STATIC 1
#include "GL/glew.h"
#include "GLFW/glfw3.h"

namespace stms::rend {
    extern bool glInfoDumped;
    extern GLint majorGlVersion;

    void initGl();


}


#endif //STONEMASON_GL_GL_HPP
