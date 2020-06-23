//
// Created by grant on 5/20/20.
//
#pragma once

#ifndef __STONEMASON_VK_WINDOW_HPP
#define __STONEMASON_VK_WINDOW_HPP

#define GLFW_INCLUDE_VULKAN
#include "GLFW/glfw3.h"

#include <vulkan/vulkan.hpp>

namespace stms {
    class VKInstance;

    class VKWindow {
    private:
        GLFWwindow *win = nullptr;
        VKInstance *parent = nullptr;

        ::vk::SurfaceKHR surf = ::vk::SurfaceKHR();

        ::vk::SwapchainKHR swapchain = ::vk::SwapchainKHR();
        ::vk::Format swapFmt = ::vk::Format();
        ::vk::Extent2D swapExtent = ::vk::Extent2D();

       std::vector<::vk::Image> swapImgs;
       std::vector<::vk::ImageView> swapImgViews;
       std::vector<::vk::Framebuffer> swapFrameBufs;

    public:
        VKWindow() = default;
        virtual ~VKWindow();

        VKWindow(VKWindow &&rhs);
        VKWindow(const VKWindow &rhs) = delete;

        VKWindow &operator=(VKWindow &&rhs);
        VKWindow &operator=(const VKWindow &rhs) = delete;
    };
}

#endif //__STONEMASON_VK_WINDOW_HPP
