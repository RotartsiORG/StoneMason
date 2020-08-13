//
// Created by grant on 6/19/20.
//
#ifdef STMS_ENABLE_OPENGL

#include "stms/rend/gl/gl_freetype.hpp"

#include <utility>
#include "stms/rend/gl/gl.hpp"
#include "stms/logging.hpp"

#include "glm/gtx/transform.hpp"

static constexpr char vertSrc[] = R"(
#version 330 core

layout (location = 0) in vec2 pos;
layout (location = 1) in vec2 inTexCoords;

out vec2 vTexCoords;

uniform mat4 userTrans;
uniform mat4 txtTrans;

void main() {
    gl_Position = userTrans * txtTrans * vec4(pos, 0.0, 1.0);
    vTexCoords = inTexCoords;
}

)";

static constexpr char fragSrc[] = R"(
#version 330 core

in vec2 vTexCoords;
out vec4 fragColor;

uniform vec4 color;
uniform sampler2D tex;

void main() {
    fragColor = vec4(color.rgb, texture(tex, vTexCoords).r * color.a);
}

)";

namespace stms {

    GLShaderProgram *GLFTFace::ftShader = nullptr;

    GLFTFace::GLFTFace(FTLibrary *lib, const char *filename, FT_Long index) : _stms_FTFace(lib, filename, index) {}

    glm::ivec2 GLFTFace::getDims(const U32String &str) {
        glm::ivec2 ret = {0, newlineAdv * spacing};
        int stage = 0;
        FT_UInt prevGlyph = 0;
        for (const auto &c : str) {
            if (!loadGlyph(c)) {continue;}

            if (c == '\n') {
                if (stage > ret.x) {
                    ret.x = stage;
                    stage = 0;
                }

                ret.y += newlineAdv * spacing;
                continue;
            }

            if (c == '\t') {
                if (!loadGlyph(' ')) { continue; }
                int target = static_cast<int>(cache[' '].metrics.horiAdvance) * tabSize;
                stage += (target - (stage % target));
                continue;
            }

            if (kern && prevGlyph) {
                FT_Vector delta;
                FT_Get_Kerning(face, prevGlyph, cache[c].index, FT_KERNING_DEFAULT, &delta);
                stage += delta.x;
            }

            prevGlyph = cache[c].index;

            stage += cache[c].metrics.horiAdvance;
        }

        if (stage > ret.x) {
            ret.x = stage;
        }

        return ret;
    }

    GLShaderProgram *GLFTFace::getTextShader() {
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

        return ftShader;
    }

    void GLFTFace::render(const U32String& str, glm::mat4 trans, glm::vec4 col) {
        getTextShader()->bind();

        ftShader->getUniform("tex").set1i(0);
        ftShader->getUniform("color").set4f(col);
        ftShader->getUniform("userTrans").setMat4(trans);

        renderWithShader(str, ftShader, [](const U32String &, unsigned) -> glm::ivec2 {
            return {0, 0}; // no-op
        });
    }

    void GLFTFace::renderWithShader(const U32String &str, GLShaderProgram *customShader, const std::function<glm::ivec2(const U32String&, unsigned)> &renderCb) {
        customShader->bind();

        GLTexture::activateSlot(0);

        glm::ivec2 pen = {0, -(newlineAdv * spacing) - (face->size->metrics.descender >> 6)};

        unsigned i = 0;
        FT_UInt prevGlyph = 0;
        for (const auto &c : str) {
            switch (c) {
                case '\n': {
                    pen.x = 0;
                    pen.y -= static_cast<int>(newlineAdv * spacing);
                    pen += renderCb(str, i++);
                    continue;
                }
                case ' ': {
                    if (!loadGlyph(' ')) { i++; continue; }
                    pen.x += cache[' '].metrics.horiAdvance;
                    pen += renderCb(str, i++);
                    continue;
                }
                case '\t': {
                    if (!loadGlyph(' ')) { i++; continue; }
                    long target = cache[' '].metrics.horiAdvance * tabSize;
                    pen.x += (target - (pen.x % target));
                    pen += renderCb(str, i++);
                    continue;
                }
                default: {
                    break;
                }
            }

            if (!loadGlyph(c)) { i++; continue; }

            if (kern && prevGlyph) {
                FT_Vector delta;
                FT_Get_Kerning(face, prevGlyph, cache[c].index, FT_KERNING_DEFAULT, &delta);
                pen.x += delta.x;
                pen.y += delta.y;
            }

            prevGlyph = cache[c].index;

            customShader->getUniform("txtTrans").setMat4(
                glm::translate(glm::vec3{pen.x + cache[c].metrics.horiBearingX, pen.y + cache[c].metrics.horiBearingY, 0}) *
                glm::scale(glm::vec3{cache[c].metrics.width, cache[c].metrics.height, 0})
            );

            cache[c].texture.bind();
            GLTexRenderer::getVao()->bind();

            glm::ivec2 additionalAdvance = renderCb(str, i++);

            STMS_GLC(glDrawArrays(GL_TRIANGLES, 0, 6));

            // Unbinding the VAO is important! Don't corrupt the VAO by unintentionally writing stuff to it!
            GLTexRenderer::getVao()->unbind();

            pen.x += cache[c].metrics.horiAdvance;
            pen += additionalAdvance;
        }
    }

    bool GLFTFace::loadGlyph(char32_t glyph) {
        if (cache.find(glyph) != cache.end()) {
            return true; // Glyph has already been cached, no work needed
        }

        STMS_GLC(glPixelStorei(GL_UNPACK_ALIGNMENT, 1)); // Should this be set back to 4 after we're done?

        if (FT_Load_Char(face, glyph, FT_LOAD_RENDER)) {
            STMS_ERROR("Failed to load glyph {}! It will be ignored!", static_cast<char>(glyph));
            STMS_GLC(glPixelStorei(GL_UNPACK_ALIGNMENT, 4));
            return false;
        }

        auto toInsert = GLFTChar{};
        toInsert.metrics = face->glyph->metrics;

        // Bit shift right by 6 to div by 64 (2^6) to convert from subpixel positions to pixel positions.
        toInsert.metrics.width >>= 6;
        toInsert.metrics.height >>= 6;

        toInsert.metrics.horiBearingY >>= 6;
        toInsert.metrics.horiBearingX >>= 6;
        toInsert.metrics.horiAdvance >>= 6;

        toInsert.metrics.vertBearingY >>= 6;
        toInsert.metrics.vertBearingX >>= 6;
        toInsert.metrics.vertAdvance >>= 6;

        toInsert.index = face->glyph->glyph_index;

        toInsert.texture.bind();
        STMS_GLC(glTexImage2D(GL_TEXTURE_2D,0, GL_RED, toInsert.metrics.width, toInsert.metrics.height,
                     0, GL_RED, GL_UNSIGNED_BYTE, face->glyph->bitmap.buffer));

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
        STMS_GLC(glPixelStorei(GL_UNPACK_ALIGNMENT, 4));
        return true;
    }

    void GLFTFace::attachFile(const char *filename) {
        if (FT_Attach_File(face, filename)) {
            STMS_ERROR("Failed to attach '{}' to face!", filename);
        }
    }
}
#endif