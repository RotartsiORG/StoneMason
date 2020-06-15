//
// Created by grant on 6/8/20.
//

#include "stms/stms.hpp"
#include "stms/rend/gl/window.hpp"
#include "stms/rend/gl/buffers.hpp"
#include "stms/rend/gl/shaders.hpp"
#include "stms/camera.hpp"

#include <fstream>
#include <stms/logging.hpp>

void clamp(float *val, float min, float max) {
    if (*val > max) {
        *val = max;
    } else if (*val < min) {
        *val = min;
    }
}

int main() {
    stms::initAll();

    {
        stms::rend::GLWindow win(640, 480);
        glfwSetInputMode(win.getRawPtr(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        glfwSetInputMode(win.getRawPtr(), GLFW_RAW_MOUSE_MOTION, GLFW_TRUE); // This is not always supported...

        double cursorX, cursorY;
        double mouseSensitivity = 1.0 / 256;
        float speed = 0.125;
        glm::vec3 camEuler = {0, 0, 0};

        glEnable(GL_DEPTH_TEST);


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

        stms::rend::GLVertexBuffer vbo(stms::rend::eDrawStatic);
        vbo.fromVector(vboDat);

        stms::rend::GLIndexBuffer ibo(stms::rend::eDrawStatic);
        ibo.fromVector(iboDat);


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

        stms::rend::GLTexture::activateSlot(0);
        stms::rend::GLTexture grassTex("./res/grass_texture.png");
        grassTex.genMipMaps();
        grassTex.setUpscale(stms::rend::eNearest);
        grassTex.setDownscale(stms::rend::eNearMipNear);
        grassTex.bind();

        stms::rend::GLUniform mvp = shaders.getUniform("mvp");
        glm::mat4 cpuMvp;

        shaders.getUniform("slot0").set1i(0); // Use texture slot 0. Why do samplers have to be uniforms?!!

        double prevX, prevY;
        glfwGetCursorPos(win.getRawPtr(), &prevX, &prevY); // Make you start looking in the right dir.

        while (!win.shouldClose()) {
            vao.bind();
            ibo.draw();


            glfwGetCursorPos(win.getRawPtr(), &cursorX, &cursorY);
            camEuler.y += -(cursorX - prevX) * mouseSensitivity;
            camEuler.x += -(cursorY - prevY) * mouseSensitivity;

            prevX = cursorX;
            prevY = cursorY;

            clamp(&camEuler.x, -90 * (3.14159265 / 180), 90 * (3.14159265 / 180));

            cam.setEuler(camEuler);
            cam.buildVecs();

            if (glfwGetKey(win.getRawPtr(), GLFW_KEY_W) == GLFW_PRESS) {
                cam.pos += cam.forward * speed;
            }
            if (glfwGetKey(win.getRawPtr(), GLFW_KEY_A) == GLFW_PRESS) {
                cam.pos += -cam.right * speed;
            }
            if (glfwGetKey(win.getRawPtr(), GLFW_KEY_S) == GLFW_PRESS) {
                cam.pos += -cam.forward * speed;
            }
            if (glfwGetKey(win.getRawPtr(), GLFW_KEY_D) == GLFW_PRESS) {
                cam.pos += cam.right * speed;
            }
            if (glfwGetKey(win.getRawPtr(), GLFW_KEY_SPACE) == GLFW_PRESS) {
                cam.pos.y += speed;
            }
            if (glfwGetKey(win.getRawPtr(), GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) {
                cam.pos.y -= speed;
            }

            if (glfwGetKey(win.getRawPtr(), GLFW_KEY_ESCAPE) == GLFW_PRESS) {
                glfwSetInputMode(win.getRawPtr(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            }

            if (glfwGetMouseButton(win.getRawPtr(), GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
                glfwSetInputMode(win.getRawPtr(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            }


            int width, height;
            glfwGetWindowSize(win.getRawPtr(), &width, &height);
            cpuMvp = cam.buildPersp((float) width / (float) height); // screw it i'm using c style casts c++ func casts are retarded
            cpuMvp *= cam.buildMatV();

            mvp.setMat4(cpuMvp);

            glViewport(0, 0, width, height);


            win.lazyFlip();
        }
    }

    stms::rend::quitGlfw();
}
