//
// Created by grant on 5/24/20.
//

#pragma once

#ifndef __STONEMASON_GL_BUFFERS_HPP
#define __STONEMASON_GL_BUFFERS_HPP

#ifdef STMS_ENABLE_OPENGL

#include "gl.hpp"

#include <vector>
#include <array>

namespace stms {
    enum GLBufferMode {
        eDrawStatic = GL_STATIC_DRAW,
        eDrawDynamic = GL_DYNAMIC_DRAW,
        eDrawStream = GL_STREAM_DRAW
    };

    enum GLBufferAccessMode {
        eRead = GL_MAP_READ_BIT,
        eWrite = GL_MAP_WRITE_BIT,

        eUnsync = GL_MAP_UNSYNCHRONIZED_BIT,
        eInvalidateBuf = GL_MAP_INVALIDATE_BUFFER_BIT,
        eInvalidateRange = GL_MAP_INVALIDATE_RANGE_BIT,
        eExplicitFlush = GL_MAP_FLUSH_EXPLICIT_BIT,
    };

    enum GLDrawMode {
        eTriangles = GL_TRIANGLES,
        eQuads = GL_QUADS,
        ePoints = GL_POINTS,
        eLineStrip = GL_LINE_STRIP,
        eLineLoop = GL_LINE_LOOP,
        eLines = GL_LINES,
        eTriStrip = GL_TRIANGLE_STRIP,
        eTriFan = GL_TRIANGLE_FAN,
        eQuadStrip = GL_QUAD_STRIP,
        ePoly = GL_POLYGON
    };

    enum GLIndexType {
        eByte = GL_UNSIGNED_BYTE,
        eShort = GL_UNSIGNED_SHORT,
        eInt = GL_UNSIGNED_INT,
    };

    template <GLenum bufType>
    class _stms_GLBuffer {
    protected:
        GLBufferMode usage = eDrawStatic;
        GLuint id = 0;
        GLsizei numElements = 0;

    public:
        _stms_GLBuffer() {
            STMS_GLC(glGenBuffers(1, &id));
        }

        virtual ~_stms_GLBuffer() {
            STMS_GLC(glDeleteBuffers(1, &id));
        }

        _stms_GLBuffer(const void *data, GLsizeiptr size, GLBufferMode mode = eDrawStatic)  : usage(mode) {
            STMS_GLC(glGenBuffers(1, &id));
            write(data, size);
        }

        explicit _stms_GLBuffer(GLBufferMode mode)  : usage(mode) {
            STMS_GLC(glGenBuffers(1, &id));
        };

        _stms_GLBuffer(const _stms_GLBuffer<bufType> &rhs) = delete;
        _stms_GLBuffer<bufType> &operator=(const _stms_GLBuffer<bufType> &rhs) = delete;

        _stms_GLBuffer(_stms_GLBuffer<bufType> &&rhs) noexcept {
            *this = std::move(rhs);
        }

        _stms_GLBuffer<bufType> &operator=(_stms_GLBuffer<bufType> &&rhs) noexcept {
            if (rhs.id == id) {
                return *this;
            }

            STMS_GLC(glDeleteBuffers(1, &id));
            id = rhs.id;
            rhs.id = 0;
            rhs.numElements = 0;

            usage = rhs.usage;
            numElements = rhs.numElements;

            return *this;
        }


        inline void bind() const {
            STMS_GLC(glBindBuffer(bufType, id));
        }

        inline void setUsage(GLBufferMode mode) {
            usage = mode;
        }

        inline void unmap() const {
            STMS_GLC(glUnmapNamedBuffer(id));
        }

        inline void flush(GLintptr offset, GLsizeiptr len) const {
            bind();
            STMS_GLC(glFlushMappedBufferRange(bufType, offset, len));
        }

        inline void write(const void *data, GLsizeiptr size) {
            bind();
            STMS_GLC(glBufferData(bufType, size, data, usage));
        };

        template <typename T>
        void fromVector(const std::vector<T> &vec, GLBufferMode mode = eDrawStatic) {
            usage = mode;
            write(vec.data(), vec.size() * sizeof(T));
            numElements = vec.size();
        }

        template <typename T, size_t S>
        void fromArray(const std::array<T, S> &arr, GLBufferMode mode = eDrawStatic) {
            usage = mode;
            write(arr.data(), S * sizeof(T));
            numElements = S;
        }

        inline void setElements(GLsizei num) { numElements = num; }

        inline void writeRange(GLintptr offset, void *data, GLsizeiptr size) {
            bind();
            STMS_GLC(glBufferSubData(bufType, offset, size, data));
        };

        [[nodiscard]] inline void *map(GLBufferAccessMode access) const {
            bind();
            return STMS_GLC(glMapBuffer(bufType, access));
        };

        [[nodiscard]] inline void *mapRange(GLintptr offset, GLsizeiptr len, GLBufferAccessMode access) const {
            bind();
            return STMS_GLC(glMapBufferRange(bufType, offset, len, access));
        };
    };

    class GLVertexBuffer : public _stms_GLBuffer<GL_ARRAY_BUFFER> {
    public:
        GLVertexBuffer();
        ~GLVertexBuffer() = default;

        GLVertexBuffer(GLVertexBuffer &&rhs) noexcept;
        GLVertexBuffer &operator=(GLVertexBuffer &&rhs) noexcept;

        GLVertexBuffer(const void *data, GLsizeiptr size, GLBufferMode mode = eDrawStatic);
        explicit GLVertexBuffer(GLBufferMode mode);

        inline void draw(GLDrawMode mode = eTriangles, GLint first = 0) {
            bind();
            STMS_GLC(glDrawArrays(mode, first, numElements));
        };

        inline void drawInstanced(GLsizei num = 1, GLDrawMode mode = eTriangles, GLint first = 0) {
            bind();
            STMS_GLC(glDrawArraysInstanced(mode, first, numElements, num));
        };
    };

    class GLIndexBuffer : public _stms_GLBuffer<GL_ELEMENT_ARRAY_BUFFER> {
    private:
        GLenum type = GL_UNSIGNED_INT;
    public:
        GLIndexBuffer();
        ~GLIndexBuffer() = default;

        GLIndexBuffer(GLIndexBuffer &&rhs) noexcept;
        GLIndexBuffer &operator=(GLIndexBuffer &&rhs) noexcept;

        GLIndexBuffer(const void *dat, GLsizeiptr size, GLBufferMode mode = eDrawStatic);
        explicit GLIndexBuffer(GLBufferMode mode);

        inline void setIndexType(GLIndexType newType) {
            type = newType;
        };

        inline void draw(GLDrawMode mode = eTriangles) {
            bind();
            STMS_GLC(glDrawElements(mode, numElements, type, nullptr));
        };

        void drawInstanced(GLsizei num = 1, GLDrawMode mode = eTriangles) {
            bind();
            STMS_GLC(glDrawElementsInstanced(mode, numElements, type, nullptr, num));
        };
    };

    inline void localIndexedGlDrawInstanced(const void *indices, GLsizei numIndices,  GLsizei num = 1,
                                            GLIndexType indexType = eInt, GLDrawMode mode = eTriangles) {
        STMS_GLC(glDrawElementsInstanced(mode, numIndices, indexType, indices, num));
    }

    inline void localIndexedGlDraw(const void *indices, GLsizei numIndices, GLIndexType indexType = eInt,
                                   GLDrawMode mode = eTriangles) {
        STMS_GLC(glDrawElements(mode, numIndices, indexType, indices));
    }

    class GLVertexArray {
    private:
        class VertexArrayImpl {
        protected:

            struct VertexBufferLayout {
                explicit VertexBufferLayout(GLVertexBuffer *vbo);

                GLVertexBuffer *vbo;
                GLsizei stride = 0;

                struct VertexAttribute {
                    void *ptr;
                    GLuint divisor;
                    GLboolean normalized;
                    GLenum type;
                    GLint size;
                };

                std::vector<VertexAttribute> attribs;
            };

            // should this be heap-allocated? (we would save 16 bytes by doing so,
            // but then we would pay for heap-allocation and added complexity and possible segfaults)
            std::vector<VertexBufferLayout> vboLyos;

            static GLuint enabledIndices;

            void specifyLayout();

        public:
            VertexArrayImpl() = default;
            virtual ~VertexArrayImpl() = default;

            void pushVbo(GLVertexBuffer *vbo);
            void pushFloats(GLint num, bool normalized = false, GLuint divisor = 0);

            virtual void build() = 0;
            virtual void bind() = 0;
            virtual void unbind() = 0;

            inline void freeLayouts() {
                vboLyos.clear();
            }
        };

        class VertexArrayImplOGL2 : public VertexArrayImpl {
        public:
            VertexArrayImplOGL2() = default;

            void build() override;
            void bind() override;
            void unbind() override;
        };

        class VertexArrayImplOGL3 : public VertexArrayImpl {
        private:
            GLuint id = 0;
        public:
            VertexArrayImplOGL3();
            ~VertexArrayImplOGL3();

            void build() override;
            void bind() override;
            void unbind() override;
        };

        VertexArrayImpl *impl{};

    public:
        GLVertexArray();
        virtual ~GLVertexArray();

        GLVertexArray(const GLVertexArray &rhs) = delete;
        GLVertexArray &operator=(const GLVertexArray &rhs) = delete;

        GLVertexArray(GLVertexArray &&rhs) noexcept;
        GLVertexArray &operator=(GLVertexArray &&rhs) noexcept;

        inline void unbind() {
            impl->unbind();
        }

        inline void build() {
            impl->build();
        }

        inline void bind() {
            impl->bind();
        }

        inline void pushVbo(GLVertexBuffer *vbo) {
            impl->pushVbo(vbo);
        }

        inline void pushFloats(GLint num, bool normalized = false, GLuint divisor = 0) {
            impl->pushFloats(num, normalized, divisor);
        }

        inline void freeLayouts() {
            impl->freeLayouts();
        }
    };
}

#endif

#endif //__STONEMASON_GL_BUFFERS_HPP
