//
// Created by grant on 6/8/20.
//

#include "stms/stms.hpp"
#include "stms/rend/gl/window.hpp"
#include "stms/rend/gl/buffers.hpp"
#include "stms/rend/gl/shaders.hpp"
#include "stms/camera.hpp"

#include <fstream>

int main() {
    stms::initAll();

    {
        stms::rend::GLWindow win(640, 480);

        stms::Camera cam;

        std::vector<float> vboDat({
           -1, 1, -1, 0, 2 / 3.0f,
           1, 1, -1, 1, 2 / 3.0f,
           1, 1, 1, 1, 1.0,
           -1, 1, 1, 0, 1.0,

           -1, -1, -1, 0, 1 / 3.0f,
           1, -1, -1, 1, 1 / 3.0f,
           1, -1, 1, 1, 2 / 3.0f,
           -1, -1, 1, 0, 2 / 3.0f,

           -1, 1, -1, 0, 0,
           1, 1, -1, 1, 0,
           1, -1, -1, 1, 1 / 3.0f,
           -1, -1, -1, 0, 1 / 3.0f,

           -1, 1, 1, 0, 0,
           1, 1, 1, 1, 0,
           1, -1, 1, 1, 1 / 3.0f,
           -1, -1, 1, 0, 1 / 3.0f,

           1, 1, -1, 1, 0,
           1, -1, -1, 1, 1 / 3.0f,
           1, -1, 1, 0, 1 / 3.0f,
           1, 1, 1, 0, 0,

           -1, 1, -1, 1, 0,
           -1, -1, -1, 1, 1 / 3.0f,
           -1, -1, 1, 0, 1 / 3.0f,
           -1, 1, 1, 0, 0
         });

        std::vector<unsigned> iboDat({
            2, 1, 0, 2, 0, 3, // top
            4, 5, 6, 7, 4, 6, // Bottom
            8, 9, 10, 11, 8, 10,
            14, 13, 12, 14, 12, 15,
            18, 17, 16, 18, 16, 19,
            20, 21, 22, 23, 20, 22
        });

        stms::rend::GLVertexBuffer vbo(reinterpret_cast<void *>(vboDat.data()), vboDat.size() * sizeof(float));
        stms::rend::GLIndexBuffer ibo(reinterpret_cast<void *>(iboDat.data()), iboDat.size() * sizeof(float));
        ibo.setNumIndices(iboDat.size());

        stms::rend::GLVertexArray vao;
        vao.pushVbo(&vbo);
        vao.pushFloats(3); // position
        vao.pushFloats(2); // tex coords (unused)
        vao.build();

        stms::rend::GLShaderProgram shaders;
        {
            std::string vertSrc, fragSrc;
            std::ifstream vertShader("./res/shaders/default.vert");
            std::ifstream fragShader("./res/shaders/default.frag");

            vertSrc = std::string(std::istreambuf_iterator<char>(vertShader), std::istreambuf_iterator<char>());

            fragSrc = std::string(std::istreambuf_iterator<char>(fragShader), std::istreambuf_iterator<char>());

            stms::rend::GLShaderModule vert(stms::rend::eVert);
            vert.setAndCompileSrc(vertSrc.c_str());

            stms::rend::GLShaderModule frag(stms::rend::eFrag);
            frag.setAndCompileSrc(fragSrc.c_str());

            shaders.attachModule(&vert);
            shaders.attachModule(&frag);
            shaders.link();

            shaders.autoBindAttribLocs(vertSrc.c_str());
        }

        shaders.bind();

        while (!win.shouldClose()) {
            vao.bind();
            ibo.draw();

            win.lazyFlip();
        }
    }

    glfwTerminate();
}
