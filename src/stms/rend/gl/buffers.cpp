//
// Created by grant on 5/24/20.
//

#include "stms/rend/gl/buffers.hpp"

#include "stms/logging.hpp"

namespace stms::rend {
    GLuint VertexArray::VertexArrayImpl::enabledIndices = 0;
    GLint VertexArray::majorGlVersion = -1;

    void VertexArray::VertexArrayImpl::pushVbo(stms::rend::VertexBuffer *vbo) {
        vboLyos.emplace_back(VertexBufferLayout{vbo});
    }

    void VertexArray::VertexArrayImpl::pushFloats(GLint num, bool normalized, GLuint divisor) {
        if (vboLyos.empty()) {
            STMS_WARN("Tried to push floats to a vertex array without a bound VBO! Ignoring invocation!");
            return;
        }

        if (majorGlVersion == -1) {
            glGetIntegerv(GL_MAJOR_VERSION, &majorGlVersion);
        }
        if (majorGlVersion < 3 && divisor != 0) {
            STMS_WARN("Instanced rendering is not supported in OpenGL {}! Expect things to break (or even crash)!", majorGlVersion);
        }

        VertexBufferLayout &boundLyo = vboLyos[vboLyos.size() - 1];

        VertexBufferLayout::VertexAttribute newAttrib{};
        newAttrib.divisor = divisor;
        newAttrib.normalized = normalized ? GL_TRUE : GL_FALSE;
        newAttrib.ptr = reinterpret_cast<void *>(boundLyo.stride);
        newAttrib.size = num;

        boundLyo.stride += num * sizeof(GLfloat);
        boundLyo.attribs.emplace_back(newAttrib);
    }

    void VertexArray::VertexArrayImpl::specifyLayout() {
        while (enabledIndices != 0) {  // Remove leftover layout specification from previous binds
            glDisableVertexAttribArray(--enabledIndices);
        }

        for (const auto &vboLyo : vboLyos) {
            vboLyo.vbo->bind();

            for (const auto &attrib : vboLyo.attribs) {
                glEnableVertexAttribArray(enabledIndices);
                glVertexAttribPointer(enabledIndices, attrib.size, GL_FLOAT, attrib.normalized, vboLyo.stride, attrib.ptr);

                if (attrib.divisor != 0) {
                    glVertexAttribDivisor(enabledIndices, attrib.divisor);
                }

                enabledIndices++;
            }
        }
    }

    VertexArray::VertexArrayImplOGL3::VertexArrayImplOGL3() {
        glCreateVertexArrays(1, &id);
    }

    VertexArray::VertexArrayImplOGL3::~VertexArrayImplOGL3() {
        glDeleteVertexArrays(1, &id);
    }

    void VertexArray::VertexArrayImplOGL3::finalize() {
        bind();
        specifyLayout();
    }

    void VertexArray::VertexArrayImplOGL3::bind() {
        glBindVertexArray(id);
    }

    VertexArray::VertexArrayImpl::VertexBufferLayout::VertexBufferLayout(VertexBuffer *vbo) {
        this->vbo = vbo;
    }

    void VertexArray::VertexArrayImplOGL2::finalize() { /* no-op */ }

    void VertexArray::VertexArrayImplOGL2::bind() {
        specifyLayout();
    }

    VertexArray::VertexArray() {
        if (majorGlVersion == -1) {
            glGetIntegerv(GL_MAJOR_VERSION, &majorGlVersion);
        }

        if (majorGlVersion == 2) {
            impl = new VertexArrayImplOGL2();
            return;
        } else if (majorGlVersion >= 3) {
            impl = new VertexArrayImplOGL3();
            return;
        }

        STMS_WARN("OpenGL {} is outdated and untested! Expect bugs or even crashes!", majorGlVersion);
    }

    VertexArray::~VertexArray() {
        delete impl;
    }
}
