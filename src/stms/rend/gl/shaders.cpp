//
// Created by grant on 5/24/20.
//

#include "stms/rend/gl/shaders.hpp"

#include "stms/logging.hpp"
#include "stms/stms.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"

namespace stms::rend {
    GLShaderModule::GLShaderModule(stms::rend::GLShaderStage type) {
        id = glCreateShader(type);

    }

    bool GLShaderModule::setAndCompileSrc(const char *src) const {
        glShaderSource(id, 1, &src, nullptr);
        glCompileShader(id);

        GLint success;
        glGetShaderiv(id, GL_COMPILE_STATUS, &success);
        if(!success) {
            char infoLog[512];
            glGetShaderInfoLog(id, 512, nullptr, infoLog);
            STMS_PUSH_ERROR("Shader Compile failed: '{}'", infoLog);
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
        glLinkProgram(id);

        GLint success;
        glGetProgramiv(id, GL_LINK_STATUS, &success);
        if(!success) {
            char infoLog[512];
            glGetProgramInfoLog(id, 512, nullptr, infoLog);
            STMS_PUSH_ERROR("Failed to link shader program: {}", infoLog);
        }
    }

    GLShaderProgram::GLShaderProgram() {
        id = glCreateProgram();
    }

    GLShaderProgram::~GLShaderProgram() {
        glDeleteProgram(id);
        id = 0;
    }

    void GLShaderProgram::autoBindAttribLocs(const GLchar *vertShaderSrc) const {
        std::istringstream input;
        input.str(vertShaderSrc);

        int lineNum = 0;
        GLuint attribLoc = 0;
        for (std::string line; std::getline(input, line); ) {
            STMS_TRACE("Line {}: {}", lineNum, line);

            // If the line starts with attribute, we bind the location and increment the index
            if (line.rfind("attribute", 0) == 0) {
                // + 10 to begin to ignore the `attribute` qualifier.
                auto nameStart = std::find_if(line.begin() + 10, line.end(), [](char c) {
                    return std::isspace<char>(c, std::locale::classic());
                });

                // - 1 to ignore the semicolon, + 1 to ignore the space
                std::string attrName(nameStart + 1, line.end() - 1);

                STMS_TRACE("detected name '{}', loc {}", attrName, attribLoc);
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

        glDeleteProgram(id);
        id = rhs.id;
        rhs.id = 0;

        return *this;
    }

    GLTexture::GLTexture() {
        glGenTextures(1, &id);
    }

    void GLTexture::loadFromFile(const char *filename) const {

        int width = 0;
        int height = 0;
        int channels = 0;
        unsigned char *data = stbi_load(filename, &width, &height, &channels, 0);
        if (data == nullptr || width == 0 || height == 0 || channels == 0) {
            STMS_PUSH_ERROR("Failed to load GLTexture from {}!", filename);
            return;
        }

        STMS_INFO("Got {} channels. using GL_RGB", channels);

        bind();
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);

        stbi_image_free(data);
    }

    GLTexture::~GLTexture() {
        glDeleteTextures(1, &id);
    }

    void GLTexture::setUpscale(GLScaleMode mode) const {
        if (!(mode == eNearest || mode == eLinear)) {
            STMS_PUSH_ERROR("GLTexture::setUpscale() called with invalid mode {}! Only eNearest and eLinear are valid! Ignoring...", mode);
            return;
        }

        bind();
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, mode);
    }

    GLTexture::GLTexture(const char *filename) {
        glGenTextures(1, &id);
        loadFromFile(filename);
    }

    GLTexture::GLTexture(int width, int height) {
        glGenTextures(1, &id);
        bind();
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    }

    GLTexture::GLTexture(GLTexture &&rhs) noexcept {
        *this = std::move(rhs);
    }

    GLTexture &GLTexture::operator=(GLTexture &&rhs) noexcept {
        if (rhs.id == id) {
            return *this;
        }

        glDeleteTextures(1, &id);
        id = rhs.id;
        rhs.id = 0;

        return *this;
    }


    GLFrameBuffer::GLFrameBuffer(int width, int height) : tex(width, height) {
        glGenFramebuffers(1, &id);
        bind();

        // Force use of only 1 color attachment for now.
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex.id, 0);

        glGenRenderbuffers(1, &rbo);
        glBindRenderbuffer(GL_RENDERBUFFER, rbo);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rbo);

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            STMS_PUSH_ERROR("{}x{} framebuffer cannot be completed!", width, height);
        }
    }

    GLFrameBuffer::~GLFrameBuffer() {
        glDeleteFramebuffers(1, &id);
        glDeleteRenderbuffers(1, &rbo);
    }
}
