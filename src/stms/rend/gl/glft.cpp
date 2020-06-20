//
// Created by grant on 6/19/20.
//

#include "stms/rend/gl/glft.hpp"

#include <utility>
#include "stms/rend/gl/gl.hpp"
#include "stms/logging.hpp"

static constexpr char vertSrc[] = R"(
#version 120

attribute vec2 pos;
attribute vec2 inTexCoords;

varying vec2 vTexCoords;

uniform mat4 mvp;

void main() {
    gl_Position = mvp * vec4(pos, 0.0, 1.0);
    vTexCoords = inTexCoords;
}

)";

static constexpr char fragSrc[] = R"(
#version 120

varying vec2 vTexCoords;

uniform vec3 color;
uniform sampler2D tex;

void main() {
    gl_FragColor = vec4(color, texture2D(tex, vTexCoords).r);
}

)";

namespace stms::rend {

    GLShaderProgram *GLFTFace::ftShader = nullptr;

    GLFTFace::GLFTFace(FTLibrary *lib, const char *filename, FT_Long index) : _stms_FTFace(lib, filename, index) {
        uvVbo.fromArray(std::array<glm::vec2, 6>{{
             {0.0f, 0.0f },
             { 0.0f, 1.0f },
             { 1.0f, 1.0f },

             { 0.0f, 0.0f },
             { 1.0f, 1.0f },
             { 1.0f, 0.0f }
        }});


        ftVbo.write(nullptr, 6 * 2 * sizeof(float));

        ftVao.freeLayouts();
        ftVao.pushVbo(&ftVbo);
        ftVao.pushFloats(2);
        ftVao.pushVbo(&uvVbo);
        ftVao.pushFloats(2);
        ftVao.build();
    }

    glm::ivec2 GLFTFace::getDims(const std::basic_string<FT_ULong> &str) {
        glm::ivec2 ret = {0, 0};
        for (const FT_ULong &c : str) {
            if (cache.find(c) == cache.end()) {
                if (!loadGlyph(c)) {continue;}
            }

            ret += cache[c].advance;
        }

        return ret;
    }

    void GLFTFace::render(const std::basic_string<FT_ULong>& str, glm::mat4 trans, glm::vec3 col) {
        if (ftShader == nullptr) { // TODO: Async compile this?
            ftShader = new GLShaderProgram();

            auto frag = GLShaderModule(eFrag);
            frag.setAndCompileSrc(fragSrc);

            auto vert = GLShaderModule(eVert);
            vert.setAndCompileSrc(vertSrc);

            ftShader->attachModule(&vert);
            ftShader->attachModule(&frag);
            ftShader->link();

            ftShader->bindAttribLoc(0, "pos");
            ftShader->bindAttribLoc(1, "inTexCoords");
        }

        ftShader->bind();

        ftShader->getUniform("tex").set1i(0);
        ftShader->getUniform("color").set3f(col);
        ftShader->getUniform("mvp").setMat4(trans);

        renderWithShader(str, ftShader);
    }

    void GLFTFace::renderWithShader(const std::basic_string<FT_ULong> &str, GLShaderProgram *customShader) {
        customShader->bind();

        GLTexture::activateSlot(0);

        glm::ivec2 pen = {0, 0};

        for (const FT_ULong &c : str) {
            if (cache.find(c) == cache.end()) {
                if (!loadGlyph(c)) {continue;}
            }

            auto x = static_cast<float>(pen.x + cache[c].offset.x);
            auto y = static_cast<float>(pen.y - (cache[c].size.y - cache[c].offset.y));

            float w = cache[c].size.x;
            float h = cache[c].size.y;

            auto vboDat = std::array<glm::vec2, 6>{{
                 { x,     y + h,   },
                 { x,     y,       },
                 { x + w, y,       },

                 { x,     y + h,   },
                 { x + w, y,       },
                 { x + w, y + h,  }
             }};

            ftVbo.writeRange(0, vboDat.data(), 6 * 2 * sizeof(float));

            cache[c].texture.bind();
            ftVao.bind();

            glDrawArrays(GL_TRIANGLES, 0, 6);
            ftVao.unbind();

            pen += cache[c].advance;
        }
    }

    bool GLFTFace::loadGlyph(FT_ULong glyph) {
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1); // Should this be set back to 4 after we're done?

        if (FT_Load_Char(face, glyph, FT_LOAD_RENDER)) {
            STMS_PUSH_ERROR("Failed to load glyph {}! It will not be accounted for in the dim returned!", glyph);
            glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
            return false;
        }

        auto toInsert = GLFTChar{};
        toInsert.size = {face->glyph->bitmap.width, face->glyph->bitmap.rows};
        toInsert.advance = {face->glyph->advance.x >> 6, face->glyph->advance.y >> 6};
        toInsert.offset = {face->glyph->bitmap_left, face->glyph->bitmap_top};

        toInsert.texture.bind();
        glTexImage2D(GL_TEXTURE_2D,0, GL_RED, toInsert.size.x, toInsert.size.y,
                     0, GL_RED, GL_UNSIGNED_BYTE, face->glyph->bitmap.buffer);

        toInsert.texture.setWrapX(eClampToEdge);
        toInsert.texture.setWrapY(eClampToBorder);
        toInsert.texture.setUpscale(alias ? eLinear : eNearest);

        if (mipMaps) {
            toInsert.texture.genMipMaps();
            toInsert.texture.setDownscale(alias ? eLinMipLin : eNearMipNear);
        } else {
            toInsert.texture.setDownscale(alias ? eLinear : eNearest);
        }

        cache[glyph] = std::move(toInsert);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
        return true;
    }
}
