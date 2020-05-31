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
    class VertexBuffer {
    private:
        GLuint id;

        friend class VertexArray;

    public:
        VertexBuffer() = default;

        VertexBuffer(GLfloat *data, size_t size);

        inline void bind() const {
            glBindBuffer(GL_ARRAY_BUFFER, id);
        }
    };

    class IndexBuffer {
    private:
        GLuint id;

    public:
        IndexBuffer() = default;

        IndexBuffer(GLuint *data, size_t size);
    };

    class VertexArray {
    private:
        class VertexArrayImpl {
        protected:
            struct VertexBufferLayout {
                explicit VertexBufferLayout(VertexBuffer *vbo);

                VertexBuffer *vbo;
                GLsizei stride = 0;

                struct VertexAttribute {
                    void *ptr;
                    GLuint divisor;
                    GLboolean normalized;
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

            void pushVbo(VertexBuffer *vbo);
            void pushFloats(GLint num, bool normalized = false, GLuint divisor = 0);

            virtual void finalize() = 0;
            virtual void bind() = 0;
        };

        class VertexArrayImplOGL2 : public VertexArrayImpl {
        public:
            VertexArrayImplOGL2() = default;

            void finalize() override;
            void bind() override;
        };

        class VertexArrayImplOGL3 : public VertexArrayImpl {
        private:
            GLuint id = 0;
        public:
            VertexArrayImplOGL3();
            ~VertexArrayImplOGL3() override;

            void finalize() override;
            void bind() override;
        };

        VertexArrayImpl *impl;

        static GLint majorGlVersion;

    public:
        VertexArray();
        virtual ~VertexArray();

        inline void finalize() {
            impl->finalize();
        }

        inline void bind() {
            impl->bind();
        }

        inline void pushVbo(VertexBuffer *vbo) {
            impl->pushVbo(vbo);
        }

        inline void pushFloats(GLint num, bool normalized = false, GLuint divisor = 0) {
            impl->pushFloats(num, normalized, divisor);
        }
    };
}

#endif //__STONEMASON_GL_BUFFERS_HPP
