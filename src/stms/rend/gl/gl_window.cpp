//
// Created by grant on 5/24/20.
//

#include "stms/config.hpp"
#ifdef STMS_ENABLE_OPENGL

#include "stms/rend/gl/gl_window.hpp"

#include "stms/rend/gl/gl.hpp"
#include "stms/logging.hpp"

namespace stms {

    GLWindow::GLWindow(int width, int height, const char *title) {
        if (glInitialized) {
            STMS_ERROR("Failed to create window '{}': Only 1 OpenGL window is permitted!", title);
            return;
        }

        glInitialized = true;

        // compat with mac osx :|
        glfwDefaultWindowHints();
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

        win = glfwCreateWindow(width, height, title, nullptr, nullptr);
        if (!win) {
            STMS_ERROR("GLFW window creation failed! This window is unusable!");
        }

        glfwMakeContextCurrent(win);

        glewExperimental = enableExperimentalGlew;
        GLenum err = glewInit();
        if (err != GLEW_OK) {
            STMS_ERROR("GLEW failed to load OpenGL implementation! OpenGL for this window is unusable! Expect a crash!");
        }

        // enable blending & depth testing, but not face culling (but do configure it)
        // as that could make everyone confused :(
        glEnable(GL_BLEND);
        glEnable(GL_DEPTH_TEST);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        glCullFace(GL_BACK);
        glFrontFace(GL_CCW);

        // In newer versions of opengl, we can do `glGetIntegerv(GL_MAJOR_VERSION, &majorGlVersion)`, but it will
        // not work with opengl < 3
        auto vers = STMS_GLC(glGetString(GL_VERSION));
        majorGlVersion = std::stoi(std::string(reinterpret_cast<const char *>(vers)));

        auto vendor = STMS_GLC(glGetString(GL_VENDOR));
        auto gpu = STMS_GLC(glGetString(GL_RENDERER));
        auto glsl = STMS_GLC(glGetString(GL_SHADING_LANGUAGE_VERSION));

        STMS_INFO("Initialized OpenGL {} with GLEW {}", vers, glewGetString(GLEW_VERSION));
        STMS_INFO("OpenGL vendor is {} with renderer {}", vendor, gpu);
        STMS_INFO("GLSL version is {} and got major version {}", glsl, majorGlVersion);

        if (majorGlVersion < 2) {
            STMS_WARN("Your OpenGL version is outdated and unsupported! Expect crashes and/or bugs!");
        }

        // Setup Dear ImGui context
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO(); (void)io;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

        // Setup Dear ImGui style
        ImGui::StyleColorsDark();
        //ImGui::StyleColorsClassic();

        // Setup Platform/Renderer bindings
        ImGui_ImplGlfw_InitForOpenGL(win, true);
        ImGui_ImplOpenGL3_Init("#version 330 core"); // fingers crossed this works
    }

    GLWindow::~GLWindow() {
        // Cleanup
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }

    void newImGuiFrame() {
        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
    }

    void renderImGui() {
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }
}
#endif
