//
// Created by grant on 5/24/20.
//

#pragma once

#ifndef __STONEMASON_GL_SHADERS_HPP
#define __STONEMASON_GL_SHADERS_HPP

#define GLEW_STATIC
#include "GL/glew.h"
#include "GLFW/glfw3.h"

#include "glm/glm.hpp"
#include "glm/gtc/type_ptr.hpp"

namespace stms::rend {
    enum GLShaderStage {
        eVert = GL_VERTEX_SHADER,
        eFrag = GL_FRAGMENT_SHADER,
        eComp = GL_COMPUTE_SHADER,
        eTessCtrl = GL_TESS_CONTROL_SHADER,
        eTessEval = GL_TESS_EVALUATION_SHADER,
        eGeo = GL_GEOMETRY_SHADER
    };
    class GLShaderModule {
    private:
        GLuint id = 0;

        friend class GLShaderProgram;

    public:
        GLShaderModule() = default;

        bool setAndCompileSrc(const char *src) const;

        inline void destroy() {
            glDeleteShader(id);
            id = 0;
        }

        explicit GLShaderModule(GLShaderStage type);
        virtual ~GLShaderModule();

        GLShaderModule(const GLShaderModule &rhs) = delete;
        GLShaderModule &operator=(const GLShaderModule &rhs) = delete;

        GLShaderModule(GLShaderModule &&rhs) noexcept;
        GLShaderModule &operator=(GLShaderModule &&rhs) noexcept;
    };

    struct GLUniform {
        GLint id;

        GLUniform(const GLint &in) noexcept;
        GLUniform &operator=(const GLint &in) noexcept;

        inline void set1f(const GLfloat &x) const {
            glUniform1f(id, x);
        }

        inline void set2f(const glm::vec2 &val) const {
            glUniform2f(id, val.x, val.y);
        }

        inline void set3f(const glm::vec3 &val) const {
            glUniform3f(id, val.x, val.y, val.z);
        }

        inline void set4f(const glm::vec4 &val) const {
            glUniform4f(id, val.x, val.y, val.z, val.w);
        }

        inline void setFv(const GLfloat *arr, const GLsizei &size) const {
            glUniform1fv(id, size, arr);
        }

        inline void set1i(const GLint &val) const {
            glUniform1i(id, val);
        }

        inline void setMat4(const glm::mat4 &mat, const bool &transpose = false) const {
            glUniformMatrix4fv(id, 1, transpose ? GL_TRUE : GL_FALSE, glm::value_ptr(mat));
        }
    };

    class GLShaderProgram {
    private:
        GLuint id = 0;

    public:
        GLShaderProgram();
        virtual ~GLShaderProgram();

        GLShaderProgram(const GLShaderProgram &rhs) = delete;
        GLShaderProgram &operator=(const GLShaderProgram &rhs) = delete;

        GLShaderProgram(GLShaderProgram &&rhs) noexcept;
        GLShaderProgram &operator=(GLShaderProgram &&rhs) noexcept;

        inline void attachModule(const GLShaderModule *mod) const {
            glAttachShader(id, mod->id);
        }

        void link() const;

        /**
         * Automatically bind vertex attributes to deduced locations.
         * This function would assume that vertex attributes take on the form of
         *      `attribute [type] [name];`
         *
         * There must be *EXACTLY* one space between everything and *NO* leading or trailing whitespace.
         *
         * @param vertShaderSrc Source code of the vertex shader to deduce attribute locations from.
         */
        void autoBindAttribLocs(const GLchar *vertShaderSrc) const;
        inline void bindAttribLoc(GLuint loc, const GLchar *name) const {
            glBindAttribLocation(id, loc, name);
        }

        inline void bind() const {
            glUseProgram(id);
        }

        inline GLUniform getUniform(const GLchar *name) const {
            return GLUniform{glGetUniformLocation(id, name)};
        };
    };

    enum GLScaleMode {
        eNearest = GL_NEAREST,
        eLinear = GL_LINEAR,

        // Downscaling only
        eNearMipNear = GL_NEAREST_MIPMAP_NEAREST,
        eLinMipNear = GL_LINEAR_MIPMAP_NEAREST,
        eNearMipLin = GL_NEAREST_MIPMAP_LINEAR,
        eLineMipLin = GL_LINEAR_MIPMAP_LINEAR
    };

    enum GLWrapMode {
        eRepeat = GL_REPEAT,
        eMirrorRepeate = GL_MIRRORED_REPEAT,
        eBorder = GL_CLAMP_TO_BORDER,
        eClamp = GL_CLAMP_TO_EDGE
    };

    class GLTexture {
    private:
        GLuint id{};

        friend class GLFrameBuffer;

    public:
        GLTexture();
        virtual ~GLTexture();

        explicit GLTexture(const char *filename);
        GLTexture(int width, int height);

        GLTexture(const GLTexture &rhs) = delete;
        GLTexture &operator=(const GLTexture &rhs) = delete;

        GLTexture(GLTexture &&rhs) noexcept;
        GLTexture &operator=(GLTexture &&rhs) noexcept;

        void loadFromFile(const char *filename) const;

        static inline void activateSlot(unsigned slot = 0) {
            glActiveTexture(GL_TEXTURE0 + slot);
        }

        inline void setBorderCol(glm::vec4 color) const {
            // This counts on vec4's 4 floats being right next to each other in memory
            bind();
            glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, &color.r);
        }

        inline void setDownscale(GLScaleMode mode) const {
            bind();
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, mode);
        }

        void setUpscale(GLScaleMode mode) const;

        inline void setWrapX(GLWrapMode mode) const {
            bind();
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, mode);
        }

        inline void setWrapY(GLWrapMode mode) const {
            bind();
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, mode);
        }

        inline void genMipMaps() const {
            bind();
            glGenerateMipmap(GL_TEXTURE_2D);
        }

        inline void bind() const {
            glBindTexture(GL_TEXTURE_2D, id);
        }
    };

    class GLFrameBuffer {
    private:
        GLuint id{};
        GLuint rbo{};

    public:
        GLTexture tex;

        GLFrameBuffer() = default;
        GLFrameBuffer(int width, int height);
        virtual ~GLFrameBuffer();

        GLFrameBuffer(const GLFrameBuffer &rhs) = delete;
        GLFrameBuffer &operator=(const GLFrameBuffer &rhs) = delete;

        GLFrameBuffer(GLFrameBuffer &&rhs) noexcept;
        GLFrameBuffer &operator=(GLFrameBuffer &&rhs) noexcept;

        inline void bind() const {
            glBindFramebuffer(GL_FRAMEBUFFER, id);
        }

        static inline void bindDefault() {
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
        }
    };
}


#endif //__STONEMASON_SHADERS_HPP
