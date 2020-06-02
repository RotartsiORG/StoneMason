//
// Created by grant on 5/24/20.
//

#ifndef __STONEMASON_GL_BUFFERS_HPP
#define __STONEMASON_GL_BUFFERS_HPP

#define GLEW_STATIC
#include "GL/glew.h"
#include "GLFW/glfw3.h"

#include <vector>

namespace stms::rend {
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

    public:
        _stms_GLBuffer();
        virtual ~_stms_GLBuffer();

        inline void bind() const {
            glBindBuffer(bufType, id);
        }

        inline void setUsage(GLBufferMode mode) {
            usage = mode;
        }

        inline void unmap() const {
            glUnmapNamedBuffer(id);
        }

        inline void flush(GLintptr offset, GLsizeiptr len) const {
            bind();
            glFlushMappedBufferRange(bufType, offset, len);
        }

        void write(void *data, GLsizeiptr size) {
            bind();
            glBufferData(bufType, size, data, usage);
        };
        void writeRange(GLintptr offset, void *data, GLsizeiptr size) {
            bind();
            glBufferSubData(bufType, offset, size, data);
        };

        [[nodiscard]] void *map(GLBufferAccessMode access) const {
            bind();
            return glMapBuffer(bufType, access);
        };

        [[nodiscard]] void *mapRange(GLintptr offset, GLsizeiptr len, GLBufferAccessMode access) const {
            bind();
            return glMapBufferRange(bufType, offset, len, access);
        };
    };

    class GLVertexBuffer : public _stms_GLBuffer<GL_ARRAY_BUFFER> {
    private:
        GLsizei numVerts;
    public:
        inline void setNumVerts(GLsizei num) { numVerts = num; }

        inline void draw(GLDrawMode mode = eTriangles, GLint first = 0) {
            bind();
            glDrawArrays(mode, first, numVerts);
        };

        void drawInstanced(GLsizei num = 1, GLDrawMode mode = eTriangles, GLint first = 0) {
            bind();
            glDrawArraysInstanced(mode, first, numVerts, num);
        };
    };

    class GLIndexBuffer : public _stms_GLBuffer<GL_ELEMENT_ARRAY_BUFFER> {
    private:
        GLsizei numElements;
        GLenum type = GL_UNSIGNED_INT;
    public:
        inline void setNumIndices(GLsizei num) { numElements = num; }

        inline void setIndexType(GLIndexType newType) {
            type = newType;
        };

        inline void draw(GLDrawMode mode = eTriangles, GLint first = 0) {
            bind();
            glDrawElements(mode, numElements, type, nullptr);
        };

        void drawInstanced(GLsizei num = 1, GLDrawMode mode = eTriangles) {
            bind();
            glDrawElementsInstanced(mode, numElements, type, nullptr, num);
        };
    };

    inline void localIndexedGlDrawInstanced(const void *indices, GLsizei numIndices,  GLsizei num = 1,
                                            GLIndexType indexType = eInt, GLDrawMode mode = eTriangles) {
        glDrawElementsInstanced(mode, numIndices, indexType, indices, num);
    }

    inline void localIndexedGlDraw(const void *indices, GLsizei numIndices, GLIndexType indexType = eInt,
                                   GLDrawMode mode = eTriangles) {
        glDrawElements(mode, numIndices, indexType, indices);
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

            inline void freeLayouts() {
                vboLyos.clear();
            }
        };

        class VertexArrayImplOGL2 : public VertexArrayImpl {
        public:
            VertexArrayImplOGL2() = default;

            void build() override;
            void bind() override;
        };

        class VertexArrayImplOGL3 : public VertexArrayImpl {
        private:
            GLuint id = 0;
        public:
            VertexArrayImplOGL3();
            ~VertexArrayImplOGL3() override;

            void build() override;
            void bind() override;
        };

        VertexArrayImpl *impl{};

    public:
        GLVertexArray();
        virtual ~GLVertexArray();

        GLVertexArray(const GLVertexArray &rhs) = delete;
        GLVertexArray &operator=(const GLVertexArray &rhs) = delete;

        GLVertexArray(GLVertexArray &&rhs) noexcept;
        GLVertexArray &operator=(GLVertexArray &&rhs) noexcept;

        inline void finalize() {
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

#endif //__STONEMASON_GL_BUFFERS_HPP
