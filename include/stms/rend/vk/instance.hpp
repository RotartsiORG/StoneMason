//
// Created by grant on 5/20/20.
//
#pragma once

#ifndef __STONEMASON_VK_INSTANCE_HPP
#define __STONEMASON_VK_INSTANCE_HPP

#define GLFW_INCLUDE_VULKAN
#include "GLFW/glfw3.h"

#include <vulkan/vulkan.hpp>

namespace stms::rend {
    class VKInstance {
    private:
        ::vk::Instance inst = ::vk::Instance();
        ::vk::PhysicalDevice physDev = ::vk::PhysicalDevice();
        ::vk::Device dev = ::vk::Device();

        ::vk::Queue graphicsQueue = ::vk::Queue();
        ::vk::Queue presentQueue = ::vk::Queue();

        uint32_t graphicsQueueIndex = 0;
        uint32_t presentQueueIndex = 0;

        // createSemaphore()
        // createFence()

        // createCommandPool()
        // allocateCommandBuffers()
        
    public:
        VKInstance() = default;
        virtual ~VKInstance();

        VKInstance(const VKInstance &rhs) = delete;
        VKInstance(VKInstance &&rhs);

        VKInstance &operator=(const VKInstance &rhs) = delete;
        VKInstance &operator=(VKInstance &&rhs);

        // createBuffer()
    };
}

#endif //__STONEMASON_VK_INSTANCE_HPP
