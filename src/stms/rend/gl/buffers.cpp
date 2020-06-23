//
// Created by grant on 5/24/20.
//

#include "stms/rend/gl/buffers.hpp"

#include "stms/rend/gl/gl.hpp"
#include "stms/logging.hpp"

namespace stms {
    GLuint GLVertexArray::VertexArrayImpl::enabledIndices = 0;

    void GLVertexArray::VertexArrayImpl::pushVbo(stms::GLVertexBuffer *vbo) {
        vboLyos.emplace_back(VertexBufferLayout{vbo});
    }

    void GLVertexArray::VertexArrayImpl::pushFloats(GLint num, bool normalized, GLuint divisor) {
        if (vboLyos.empty()) {
            STMS_PUSH_ERROR("Tried to push floats to a vertex array without a bound VBO! Ignoring invocation!");
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
            STMS_GLC(glDisableVertexAttribArray(--enabledIndices));
        }

        for (const auto &vboLyo : vboLyos) {
            vboLyo.vbo->bind();

            for (const auto &attrib : vboLyo.attribs) {
                STMS_GLC(glEnableVertexAttribArray(enabledIndices));
                STMS_GLC(glVertexAttribPointer(enabledIndices, attrib.size, attrib.type, attrib.normalized, vboLyo.stride, attrib.ptr));

                if (attrib.divisor != 0) {
                    STMS_PUSH_WARNING("OpenGL 2 doesn't support vertex attribute divisors or "
                                      "instanced rendering! Don't use these if you want to support OpenGL 2!");

                    if (majorGlVersion > 2) {
                        STMS_GLC(glVertexAttribDivisor(enabledIndices, attrib.divisor));
                    } else {
                        STMS_PUSH_ERROR("OGL 2 doesn't support vertex attribute divisors! Expect crashes and/or bugs!");
                    }
                }

                enabledIndices++;
            }
        }
    }

    GLVertexArray::VertexArrayImplOGL3::VertexArrayImplOGL3() {
        STMS_GLC(glCreateVertexArrays(1, &id));
    }

    GLVertexArray::VertexArrayImplOGL3::~VertexArrayImplOGL3() {
        STMS_GLC(glDeleteVertexArrays(1, &id));
    }

    void GLVertexArray::VertexArrayImplOGL3::build() {
        bind();
        specifyLayout();
    }

    void GLVertexArray::VertexArrayImplOGL3::bind() {
        STMS_GLC(glBindVertexArray(id));
    }

    void GLVertexArray::VertexArrayImplOGL3::unbind() {
        STMS_GLC(glBindVertexArray(0));
    }

    GLVertexArray::VertexArrayImpl::VertexBufferLayout::VertexBufferLayout(GLVertexBuffer *vbo) {
        this->vbo = vbo;
    }

    void GLVertexArray::VertexArrayImplOGL2::build() { /* no-op */ }

    void GLVertexArray::VertexArrayImplOGL2::bind() {
        specifyLayout();
    }

    void GLVertexArray::VertexArrayImplOGL2::unbind() {
        while (enabledIndices != 0) {  // Remove leftover layout specification from previous binds
            STMS_GLC(glDisableVertexAttribArray(--enabledIndices));
        }
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
        if (rhs.impl == impl) {
            return *this;
        }

        delete impl;
        impl = rhs.impl;
        rhs.impl = nullptr;

        return *this;
    }

    GLVertexBuffer::GLVertexBuffer(const void *data, GLsizeiptr size, GLBufferMode mode) : _stms_GLBuffer(data, size,
                                                                                                          mode) {}

    GLVertexBuffer::GLVertexBuffer(GLBufferMode mode) : _stms_GLBuffer(mode) {}

    GLVertexBuffer::GLVertexBuffer() : _stms_GLBuffer() {}

    GLVertexBuffer::GLVertexBuffer(GLVertexBuffer &&rhs) noexcept {
        *this = std::move(rhs);
    }

    GLVertexBuffer &GLVertexBuffer::operator=(GLVertexBuffer &&rhs) noexcept {
        _stms_GLBuffer<GL_ARRAY_BUFFER>::operator=(std::move(rhs));
        return *this;
    }

    GLIndexBuffer::GLIndexBuffer(const void *dat, GLsizeiptr size, GLBufferMode mode) : _stms_GLBuffer(dat, size,
                                                                                                       mode) {}

    GLIndexBuffer::GLIndexBuffer(GLIndexBuffer &&rhs) noexcept {
        *this = std::move(rhs);
    }

    GLIndexBuffer &GLIndexBuffer::operator=(GLIndexBuffer &&rhs) noexcept {
        if (rhs.type == type) {
            return *this;
        }

        type = rhs.type;
        _stms_GLBuffer<GL_ELEMENT_ARRAY_BUFFER>::operator=(std::move(rhs));

        return *this;
    }

    GLIndexBuffer::GLIndexBuffer(GLBufferMode mode) : _stms_GLBuffer(mode) {}

    GLIndexBuffer::GLIndexBuffer() : _stms_GLBuffer() {}
}
