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

namespace stms {

    /**
     * This is not thread safe!!! TODO: add a mutex for accessing static members!
     */
    class GLFTFace : public _stms_FTFace {
    private:
        struct GLFTChar {
            FT_UInt index;
            FT_Glyph_Metrics metrics;
            GLTexture texture = GLTexture();
        };

        static GLShaderProgram *ftShader;
        std::unordered_map<char32_t, GLFTChar> cache;

        bool alias = false;
        bool mipMaps = false;

    public:

        GLFTFace(FTLibrary *lib, const char *filename, FT_Long index = 0);

        bool loadGlyph(char32_t glyph);

        inline void setAlias(bool newVal) {
            alias = newVal;
        }

        static inline GLShaderProgram *getTextShader();

        inline void setMipMaps(bool newVal) {
            mipMaps = newVal;
        }

        void render(const U32String& str, glm::mat4 trans = glm::mat4(1.0f), glm::vec4 col = {0, 0, 0, 1});

        /**
         * Allows you to write a custom text-rendering shader. In the shader, you will have access to the following:
         *     1. The 1-bit texture (only the red channel) for the glyph that is to be rendered is bound to slot 0.
         *        The red channel is a value between 0 and 1 and could be used as the alpha value of the final output.
         *     2. The position for the glyph to be rendered at could be found at vertex attribute location 0.
         *     3. The UV value for sampling the texture (see above) could be found at vertex attrib location 1.
         *     4. A mat4 uniform `txtTrans` that must be applied to the position.
         *
         * @param str Text to render
         * @param customShader Shader to use
         * @param cb Rendering callback called each time a character is rendered. It must return a `glm::ivec2`
         *          that signifies by how much the pen should be moved (in addition to advances & kerning).
         *          If unsure, just return `{0, 0}`. The `U32String` parameter is the string currently being rendered,
         *          and the `unsigned` parameter is the character currently being rendered
         *          (indexed into the `U32String` param). Note that when it is called, it is not always guarenteed
         *          that a draw call will happen. To optimize for this, you can skip setting uniforms if the
         *          character that is being drawn is whitespace.
         */
        void renderWithShader(const U32String &str, GLShaderProgram *customShader, const std::function<glm::ivec2(const U32String&, unsigned)> &rendCb);

        void attachFile(const char *filename);

        /**
         * Get the size of a certain string
         * @param str The string to get the size of
         * @return The size of the string.
         */
        glm::ivec2 getDims(const U32String &str);

        /**
         * Save memory by freeing the face and depending purely on the cache.
         */
        inline void freeFace() {
            FT_Done_Face(face);
            face = nullptr;
        }

        inline void clearCache() {
            cache.clear();
        }
    };
}

#endif //STONEMASON_GL_GLFT_HPP
