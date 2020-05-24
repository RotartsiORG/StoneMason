//
// Created by grant on 5/20/20.
//
#pragma once

#ifndef __STONEMASON_VK_WINDOW_HPP
#define __STONEMASON_VK_WINDOW_HPP

#define GLFW_INCLUDE_VULKAN
#include "GLFW/glfw3.h"

#include <vulkan/vulkan.hpp>

namespace stms::vk {
    class Instance;

    class Window {
    private:
        GLFWwindow *win = nullptr;
        Instance *parent = nullptr;

        ::vk::SurfaceKHR surf = ::vk::SurfaceKHR();

        ::vk::SwapchainKHR swapchain = ::vk::SwapchainKHR();
        ::vk::Format swapFmt = ::vk::Format();
        ::vk::Extent2D swapExtent = ::vk::Extent2D();

       std::vector<::vk::Image> swapImgs;
       std::vector<::vk::ImageView> swapImgViews;
       std::vector<::vk::Framebuffer> swapFrameBufs;

    public:
        Window() = default;
        virtual ~Window();

        Window(Window &&rhs);
        Window(const Window &rhs) = delete;

        Window &operator=(Window &&rhs);
        Window &operator=(const Window &rhs) = delete;
    };
}

#endif //__STONEMASON_VK_WINDOW_HPP
