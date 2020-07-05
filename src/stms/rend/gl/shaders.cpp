//
// Created by grant on 5/24/20.
//

#include "stms/config.hpp"

#ifdef STMS_ENABLE_OPENGL
#include "stms/rend/gl/shaders.hpp"

#include "stms/logging.hpp"
#include "stms/stms.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"


constexpr char texRendFragSrc[] = R"(
#version 120

varying vec2 vTexCoords;

uniform sampler2D slot0_tex;

void main() {
    gl_FragColor = texture2D(slot0_tex, vTexCoords);
   // gl_FragColor = vec4(0, 1, 0, 1);
}

)";

constexpr char texRendVertSrc[] = R"(
#version 120

attribute vec2 pos;
attribute vec2 inTexCoords;

varying vec2 vTexCoords;

uniform mat4 mvp;

void main() {
    gl_Position = mvp * vec4(pos.x, pos.y, 0.0, 1.0);
    vTexCoords = inTexCoords;
}
)";


namespace stms {

    GLVertexArray *GLTexRenderer::texRendVao = nullptr;
    GLVertexBuffer *GLTexRenderer::texRendVbo = nullptr;
    GLShaderProgram *GLTexRenderer::texRendShaders = nullptr;

    GLShaderModule::GLShaderModule(stms::GLShaderStage type) {
        id = STMS_GLC(glCreateShader(type));
    }

    bool GLShaderModule::setAndCompileSrc(const char *src) const {
        STMS_GLC(glShaderSource(id, 1, &src, nullptr));
        STMS_GLC(glCompileShader(id));

        GLint success;
        STMS_GLC(glGetShaderiv(id, GL_COMPILE_STATUS, &success));
        if(!success) {
            char infoLog[512];
            STMS_GLC(glGetShaderInfoLog(id, 512, nullptr, infoLog));
            STMS_ERROR("Shader Compile failed: '{}'", infoLog);
        }

        return success;
    }

    GLShaderModule::~GLShaderModule() {
        destroy();
    }

    GLShaderModule::GLShaderModule(GLShaderModule &&rhs) noexcept {
        *this = std::move(rhs);
    }

    GLShaderModule &GLShaderModule::operator=(GLShaderModule &&rhs) noexcept {
        if (rhs.id == id) {
            return *this;
        }

        destroy();
        id = rhs.id;
        rhs.id = 0;

        return *this;
    }

    GLUniform::GLUniform(const GLint &in) noexcept : id(in) {}

    GLUniform &GLUniform::operator=(const GLint &in) noexcept {
        id = in;
        return *this;
    }

    void GLShaderProgram::link() const {
        STMS_GLC(glLinkProgram(id));

        GLint success;
        STMS_GLC(glGetProgramiv(id, GL_LINK_STATUS, &success));
        if(!success) {
            char infoLog[512];
            STMS_GLC(glGetProgramInfoLog(id, 512, nullptr, infoLog));
            STMS_ERROR("Failed to link shader program: {}", infoLog);
        }
    }

    GLShaderProgram::GLShaderProgram() {
        id = STMS_GLC(glCreateProgram());
    }

    GLShaderProgram::~GLShaderProgram() {
        STMS_GLC(glDeleteProgram(id));
        id = 0;
    }

    void GLShaderProgram::autoBindAttribLocs(const GLchar *vertShaderSrc) const {
        std::istringstream input;
        input.str(vertShaderSrc);

        int lineNum = 0;
        GLuint attribLoc = 0;
        for (std::string line; std::getline(input, line); ) {
            // If the line starts with attribute, we bind the location and increment the index
            if (line.rfind("attribute", 0) == 0) {
                // + 10 to begin to ignore the `attribute` qualifier.
                auto nameStart = std::find_if(line.begin() + 10, line.end(), [](char c) {
                    return std::isspace<char>(c, std::locale::classic());
                });

                // - 1 to ignore the semicolon, + 1 to ignore the space
                std::string attrName(nameStart + 1, line.end() - 1);

                STMS_TRACE("Detected attrib '{}', loc {} on line {}: '{}'", attrName, attribLoc, lineNum, line);
                bindAttribLoc(attribLoc++, attrName.c_str());
            }

            lineNum++;
        }
    }

    GLShaderProgram::GLShaderProgram(GLShaderProgram &&rhs) noexcept {
        *this = std::move(rhs);
    }

    GLShaderProgram &GLShaderProgram::operator=(GLShaderProgram &&rhs) noexcept {
        if (id == rhs.id) {
            return *this;
        }

        STMS_TRACE("Del shader {} operator=", id);
        STMS_GLC(glDeleteProgram(id));
        id = rhs.id;
        rhs.id = 0;

        return *this;
    }

    GLTexture::GLTexture() {
        STMS_GLC(glGenTextures(1, &id));
        setUpscale(eNearest);
        setDownscale(eNearest);
    }

    void GLTexture::loadFromFile(const char *filename) const {

        int width = 0;
        int height = 0;
        int channels = 0;
        unsigned char *data = stbi_load(filename, &width, &height, &channels, 0);
        if (data == nullptr || width == 0 || height == 0 || channels == 0) {
            STMS_ERROR("Failed to load GLTexture from {}!", filename);
            return;
        }

        STMS_DEBUG("Got {} channels. Using GL_RGBA if 4 GL_RGB otherwise", channels);

        bind();
        GLenum format = channels == 4 ? GL_RGBA : GL_RGB;
        STMS_GLC(glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data));

        stbi_image_free(data);
    }

    GLTexture::~GLTexture() {
        STMS_GLC(glDeleteTextures(1, &id));
    }

    void GLTexture::setUpscale(GLScaleMode mode) const {
        if (!(mode == eNearest || mode == eLinear)) {
            STMS_ERROR("GLTexture::setUpscale() called with invalid mode {}! Only eNearest and eLinear are valid! Ignoring...", mode);
            return;
        }

        bind();
        STMS_GLC(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, mode));
    }

    GLTexture::GLTexture(const char *filename) {
        STMS_GLC(glGenTextures(1, &id));
        loadFromFile(filename);
        setUpscale(eNearest);
        setDownscale(eNearest);
    }

    GLTexture::GLTexture(int width, int height) {
        STMS_GLC(glGenTextures(1, &id));
        bind();
        STMS_GLC(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr));
        setUpscale(eNearest);
        setDownscale(eNearest);
    }

    GLTexture::GLTexture(GLTexture &&rhs) noexcept {
        *this = std::move(rhs);
    }

    GLTexture &GLTexture::operator=(GLTexture &&rhs) noexcept {
        if (rhs.id == id) {
            return *this;
        }

        STMS_GLC(glDeleteTextures(1, &id));
        id = rhs.id;
        rhs.id = 0;

        return *this;
    }


    GLFrameBuffer::GLFrameBuffer(int width, int height) : tex(width, height) {
        STMS_GLC(glGenFramebuffers(1, &id));
        bind();

        // Force use of only 1 color attachment for now.
        STMS_GLC(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex.id, 0));

        STMS_GLC(glGenRenderbuffers(1, &rbo));
        STMS_GLC(glBindRenderbuffer(GL_RENDERBUFFER, rbo));
        STMS_GLC(glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height));
        STMS_GLC(glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rbo));

        auto complete = STMS_GLC(glCheckFramebufferStatus(GL_FRAMEBUFFER));
        if (complete != GL_FRAMEBUFFER_COMPLETE) {
            STMS_ERROR("{}x{} framebuffer cannot be completed!", width, height);
        }
    }

    GLFrameBuffer::~GLFrameBuffer() {
        STMS_GLC(glDeleteFramebuffers(1, &id));
        STMS_GLC(glDeleteRenderbuffers(1, &rbo));
    }

    GLFrameBuffer::GLFrameBuffer(GLFrameBuffer &&rhs) noexcept {
        *this = std::move(rhs);
    }

    GLFrameBuffer &GLFrameBuffer::operator=(GLFrameBuffer &&rhs) noexcept {
        // Using || bc these attr should ALWAYS be distinct for any GLFrameBuffer, otherwise someone did something
        // incredibly stupid.
        if (tex.id == rhs.tex.id || rbo == rhs.rbo || id == rhs.id) {
            return *this;
        }

        STMS_GLC(glDeleteFramebuffers(1, &id));
        STMS_GLC(glDeleteRenderbuffers(1, &rbo));

        id = rhs.id;
        rbo = rhs.rbo;

        rhs.id = 0;
        rhs.rbo = 0;

        tex = std::move(rhs.tex);

        return *this;
    }

    GLVertexArray *GLTexRenderer::getVao() {
        if (texRendVao == nullptr) {
            texRendVao = new GLVertexArray();

            texRendVao->pushVbo(getVbo());
            texRendVao->pushFloats(2);
            texRendVao->pushFloats(2);
            texRendVao->build();
        }

        return texRendVao;
    }

    GLVertexBuffer *GLTexRenderer::getVbo() {
        if (texRendVbo == nullptr) {
            texRendVbo = new GLVertexBuffer(eDrawStatic);
            texRendVbo->fromArray(std::array<glm::vec4, 6>{{
                { 0,  0,  0,  0  },
                { 1,  0,  1,  0  },
                { 1, -1,  1,  1  },

                { 0,  0,  0,  0  },
                { 0, -1,  0,  1  },
                { 1, -1,  1,  1  }
            }});
        }

        return texRendVbo;
    }

    GLShaderProgram *GLTexRenderer::getShaders() {
        if (texRendShaders == nullptr) {
            texRendShaders = new GLShaderProgram();

            auto frag = GLShaderModule(eFrag);
            frag.setAndCompileSrc(texRendFragSrc);

            auto vert = GLShaderModule(eVert);
            vert.setAndCompileSrc(texRendVertSrc);

            texRendShaders->attachModule(&vert);
            texRendShaders->attachModule(&frag);
            texRendShaders->link();

            texRendShaders->bindAttribLoc(0, "pos");
            texRendShaders->bindAttribLoc(1, "inTexCoords");
        }

        return texRendShaders;
    }

    void GLTexRenderer::renderTexture(GLTexture *tex, const glm::mat4 &transforms) {
        getShaders()->bind();
        texRendShaders->getUniform("mvp").setMat4(transforms);
        texRendShaders->getUniform("slot0_tex").set1i(0);

        GLTexture::activateSlot(0);
        tex->bind();

        getVao()->bind();
        getVbo()->draw();
    }
}

#endif