//
// Created by grant on 6/19/20.
//

#pragma once

#ifndef STONEMASON_GL_GLFT_HPP
#define STONEMASON_GL_GLFT_HPP

#include "stms/freetype.hpp"

#include <unordered_map>

#include "shaders.hpp"
#include "buffers.hpp"

namespace stms::rend {
    class GLFTFace : public _stms_FTFace {
    private:
        struct GLFTChar {
            glm::ivec2 size;
            glm::ivec2 offset;
            glm::ivec2 advance;
            GLTexture texture = GLTexture();
        };

        static GLShaderProgram *ftShader;
        std::unordered_map<FT_ULong, GLFTChar> cache;

        bool alias = false;
        bool mipMaps = false;

        // TODO: Should these properties be static?
        GLVertexArray ftVao = GLVertexArray();
        GLVertexBuffer ftVbo = GLVertexBuffer(eDrawDynamic);
        GLVertexBuffer uvVbo = GLVertexBuffer(eDrawStatic);

    public:

        GLFTFace(FTLibrary *lib, const char *filename, FT_Long index = 0);

        bool loadGlyph(FT_ULong glyph);

        inline void setAlias(bool newVal) {
            alias = newVal;
        }

        inline void setMipMaps(bool newVal) {
            mipMaps = newVal;
        }

        void render(const std::basic_string<FT_ULong>& str, glm::mat4 trans = glm::mat4(1.0f),
                 glm::vec3 col = {0, 0, 0});

        void renderWithShader(const std::basic_string<FT_ULong> &str, GLShaderProgram *customShader);

        /**
         * Get the size of a certain string
         * @param str The string to get the size of
         * @return The size of the string. NOTE: THIS CAN BE NEGATIVE!!! A NEGATIVE VALUE SIGNIFIES THAT YOU MUSH
         *         START THE PEN IN THE OPPOSITE CORNER!!
         */
        glm::ivec2 getDims(const std::basic_string<FT_ULong> &str);


        inline void clearCaches() {
            cache.clear();
        }
    };
}

#endif //STONEMASON_GL_GLFT_HPP
