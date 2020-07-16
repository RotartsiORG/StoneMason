// IMPORTANT: This is spaghetti code and is only used for testing the ogl api as it is developed.
// THIS CODE IS VERY BAD! A NEW OPENGL DEMO WITH THE AIM OF DEMONSTRATING THE API WILL BE WRITTEN!!

//
// Created by grant on 6/8/20.
//

#include "stms/stms.hpp"
#include "stms/rend/gl/gl_window.hpp"
#include "stms/rend/gl/gl_buffers.hpp"
#include "stms/rend/gl/gl_shaders.hpp"
#include "stms/camera.hpp"
#include "stms/rend/gl/gl_freetype.hpp"

#include <fstream>
#include <stms/logging.hpp>
#include <stms/rend/gl/gl.hpp>

#include "stms/timers.hpp"

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
        stms::GLWindow win(640, 480);
        glfwSetInputMode(win.getRawPtr(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        if (glfwRawMouseMotionSupported()) {
            glfwSetInputMode(win.getRawPtr(), GLFW_RAW_MOUSE_MOTION, GLFW_TRUE); // This is not always supported...
        } else {
            STMS_WARN("Raw mouse input not supported! first person camera may be broken :/");
        }

        double cursorX, cursorY;
        double mouseSensitivity = 1.0 / 256;
        float speed = 0.125;
        glm::vec3 camEuler = {0, 0, 0};


//        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

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

        stms::GLVertexBuffer vbo(stms::eDrawStatic);
        vbo.fromVector(vboDat);

        stms::GLIndexBuffer ibo(stms::eDrawStatic);
        ibo.fromVector(iboDat);

        stms::GLVertexArray vao;
        vao.pushVbo(&vbo);
        vao.pushFloats(3); // position
        vao.pushFloats(2); // tex coords (unused)
        vao.build();

        stms::GLShaderProgram shaders;
        {
            std::string vertSrc, fragSrc;
            std::ifstream vertShader("./res/shaders/default.vert");
            std::ifstream fragShader("./res/shaders/default.frag");

            vertSrc = std::string(std::istreambuf_iterator<char>(vertShader), std::istreambuf_iterator<char>());

            fragSrc = std::string(std::istreambuf_iterator<char>(fragShader), std::istreambuf_iterator<char>());

            stms::GLShaderModule vert(stms::eVert);
            vert.setAndCompileSrc(vertSrc.c_str());

            stms::GLShaderModule frag(stms::eFrag);
            frag.setAndCompileSrc(fragSrc.c_str());

            shaders.attachModule(&vert);
            shaders.attachModule(&frag);
            shaders.link();

            shaders.autoBindAttribLocs(vertSrc.c_str());
        }

        shaders.bind();

        stms::GLTexture::activateSlot(0);
        stms::GLTexture grassTex("./res/grass_texture.png");
        grassTex.genMipMaps();
        grassTex.setUpscale(stms::eNearest);
        grassTex.setDownscale(stms::eNearMipNear);
        grassTex.bind();

        stms::GLUniform mvp = shaders.getUniform("mvp");
        glm::mat4 cpuMvp;

        shaders.getUniform("slot0").set1i(0); // Use texture slot 0. Why do samplers have to be uniforms?!!

        double prevX, prevY;
        glfwGetCursorPos(win.getRawPtr(), &prevX, &prevY); // Make you start looking in the right dir.

        auto gnu_unifont = stms::GLFTFace(&stms::defaultFtLib(), "./res/gnu-unifont-13.0.02.ttf");
        gnu_unifont.setSpacing(1.2908);
        gnu_unifont.setSize(39);

        stms::U32String toRend = U"FPS: 60\nMSPT: 16";
        glm::ivec2 txtSize = gnu_unifont.getDims(toRend);
        glfwSetWindowSize(win.getRawPtr(), txtSize.x, txtSize.y);

        glm::mat4 fontMat = glm::translate(glm::vec3{0, txtSize.y, 0});

        stms::GLFrameBuffer frameBuf = stms::GLFrameBuffer(txtSize.x, txtSize.y);

        glm::ivec2 size = win.getSize();
        stms::TPSTimer tpsTimer;
        tpsTimer.tick();
        while (!win.shouldClose()) {
            stms::newImGuiFrame();

            frameBuf.bind();
            stms::enableGl(stms::eDepthTest);
            stms::clearGl(stms::eDepth | stms::eColor);

            tpsTimer.tick();
            toRend = U"FPS: ";

            auto fpsStr = std::to_string(tpsTimer.getLatestTps());
            toRend.append(fpsStr.begin(), fpsStr.end());
            toRend += U"\nMgSPT: ";
            fpsStr = std::to_string(tpsTimer.getLatestMspt());
            toRend.append(fpsStr.begin(), fpsStr.end());
            tpsTimer.wait(60);

            glm::mat4 proj = glm::ortho(0.0f, (float) size.x, 0.0f, (float) size.y);

            grassTex.bind();
            shaders.bind();
            vao.bind();
            ibo.draw();
            vao.unbind();

            gnu_unifont.render(toRend, proj * fontMat, {1.0, 1.0, 0, 1.0});

            stms::GLFrameBuffer::bindDefault();
            stms::viewportGl(0, 0, size.x, size.y);
            stms::clearGl(stms::eColor);
            stms::disableGl(stms::eDepthTest);

            stms::GLTexRenderer::renderTexture(&frameBuf.tex);
            ImGui::Begin("test");
            ImGui::Text("test");
            ImGui::End();

            stms::renderImGui();
            stms::flushGlErrs("something");


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

            size = win.getSize();
            cpuMvp = cam.buildPersp((float) size.x / (float) size.y); // screw it i'm using c style casts c++ func casts are retarded
            cpuMvp *= cam.buildMatV();

            frameBuf = stms::GLFrameBuffer(size.x, size.y);

            shaders.bind();
            mvp.setMat4(cpuMvp);

            stms::pollEvents();
            win.flip();
        }

        stms::flushGlErrs("Terminate");
    }
}
