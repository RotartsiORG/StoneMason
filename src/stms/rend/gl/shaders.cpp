//
// Created by grant on 5/24/20.
//

#include "stms/rend/gl/shaders.hpp"

#include "stms/logging.hpp"
#include "stms/util.hpp"

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
            STMS_WARN("Shader Compile failed: '{}'", infoLog);
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
            STMS_WARN("Failed to link shader program: {}", infoLog);
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

                STMS_TRACE("detected name '{}'", attrName);
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
}
