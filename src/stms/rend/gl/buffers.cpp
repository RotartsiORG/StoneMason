//
// Created by grant on 5/24/20.
//

#include "stms/rend/gl/buffers.hpp"

#include "stms/rend/gl/gl.hpp"
#include "stms/logging.hpp"

namespace stms::rend {
    GLuint GLVertexArray::VertexArrayImpl::enabledIndices = 0;

    void GLVertexArray::VertexArrayImpl::pushVbo(stms::rend::GLVertexBuffer *vbo) {
        vboLyos.emplace_back(VertexBufferLayout{vbo});
    }

    void GLVertexArray::VertexArrayImpl::pushFloats(GLint num, bool normalized, GLuint divisor) {
        if (vboLyos.empty()) {
            STMS_WARN("Tried to push floats to a vertex array without a bound VBO! Ignoring invocation!");
            return;
        }

        VertexBufferLayout &boundLyo = vboLyos[vboLyos.size() - 1];

        VertexBufferLayout::VertexAttribute newAttrib{};
        newAttrib.divisor = divisor;
        newAttrib.normalized = normalized ? GL_TRUE : GL_FALSE;
        newAttrib.ptr = reinterpret_cast<void *>(boundLyo.stride);
        newAttrib.size = num;
        newAttrib.type = GL_FLOAT;

        boundLyo.stride += num * sizeof(GLfloat);
        boundLyo.attribs.emplace_back(newAttrib);
    }

    void GLVertexArray::VertexArrayImpl::specifyLayout() {
        while (enabledIndices != 0) {  // Remove leftover layout specification from previous binds
            glDisableVertexAttribArray(--enabledIndices);
        }

        for (const auto &vboLyo : vboLyos) {
            vboLyo.vbo->bind();

            for (const auto &attrib : vboLyo.attribs) {
                glEnableVertexAttribArray(enabledIndices);
                glVertexAttribPointer(enabledIndices, attrib.size, attrib.type, attrib.normalized, vboLyo.stride, attrib.ptr);

                if (attrib.divisor != 0) {
                    STMS_WARN("OpenGL 2 doesn't support non-zero vertex attribute divisors!");
                    STMS_WARN("Don't use instanced rendering if you wish to maintain backwards compatibility!");

                    if (majorGlVersion > 2) {
                        glVertexAttribDivisor(enabledIndices, attrib.divisor);
                    }
                }

                enabledIndices++;
            }
        }
    }

    GLVertexArray::VertexArrayImplOGL3::VertexArrayImplOGL3() {
        glCreateVertexArrays(1, &id);
    }

    GLVertexArray::VertexArrayImplOGL3::~VertexArrayImplOGL3() {
        glDeleteVertexArrays(1, &id);
    }

    void GLVertexArray::VertexArrayImplOGL3::build() {
        bind();
        specifyLayout();
    }

    void GLVertexArray::VertexArrayImplOGL3::bind() {
        glBindVertexArray(id);
    }

    GLVertexArray::VertexArrayImpl::VertexBufferLayout::VertexBufferLayout(GLVertexBuffer *vbo) {
        this->vbo = vbo;
    }

    void GLVertexArray::VertexArrayImplOGL2::build() { /* no-op */ }

    void GLVertexArray::VertexArrayImplOGL2::bind() {
        specifyLayout();
    }

    GLVertexArray::GLVertexArray() {
        if (majorGlVersion < 3) {
            impl = new VertexArrayImplOGL2();
        } else {
            impl = new VertexArrayImplOGL3();
        }
    }

    GLVertexArray::~GLVertexArray() {
        delete impl;
    }

    GLVertexArray::GLVertexArray(GLVertexArray &&rhs) noexcept {
        *this = std::move(rhs);
    }

    GLVertexArray &GLVertexArray::operator=(GLVertexArray &&rhs) noexcept {
        if (&rhs == this) {
            return *this;
        }

        delete impl;
        impl = rhs.impl;
        rhs.impl = nullptr;

        return *this;
    }

    template<GLenum bufType>
    _stms_GLBuffer<bufType>::_stms_GLBuffer() {
        glCreateBuffers(1, &id);
    }

    template<GLenum bufType>
    _stms_GLBuffer<bufType>::~_stms_GLBuffer() {
        glDeleteBuffers(1, &id);
    }

}
