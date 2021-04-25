//
// Created by grant on 7/5/20.
//

#pragma once

#ifndef __STONEMASON_VK_WINDOW_HPP
#define __STONEMASON_VK_WINDOW_HPP

#ifdef STMS_ENABLE_VULKAN

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <utility>
#include <vulkan/vulkan.hpp>

#include "stms/rend/window.hpp"
#include "stms/rend/vk/vk_instance.hpp"

#include <array>
#include <unordered_set>
#include <stms/logging.hpp>

namespace stms {

    class VKDevice {
    public:
        vk::Device device;
        vk::Queue graphics;
        vk::Queue present;
        vk::Queue compute;

        VKInstance *pInst{};

        VKGPU phys;

        vk::PhysicalDeviceFeatures feats;
        std::unordered_set<std::string> enabledExts;

    public:

        struct ConstructionDetails {
            ConstructionDetails() noexcept = default;

            ConstructionDetails(const vk::PhysicalDeviceFeatures &req, const vk::PhysicalDeviceFeatures &want,
                    std::unordered_set<std::string> wantExt, std::vector<const char *> reqExt);

            vk::PhysicalDeviceFeatures requiredFeats;
            vk::PhysicalDeviceFeatures wantedFeats;

            std::unordered_set<std::string> wantedExts;
            std::vector<const char *> requiredExts = std::vector<const char *>({VK_KHR_SWAPCHAIN_EXTENSION_NAME});
        };

        VKDevice(VKInstance *inst, VKGPU dev, const ConstructionDetails& details = {});
        virtual ~VKDevice();

        inline bool extEnabled(const std::string &ext) {
            return enabledExts.find(ext) != enabledExts.end();
        }
    };


    class VKWindow;

    class VKPartialWindow : public _stms_GenericWindow {
    protected:
        VKPartialWindow() = default;
    public:
        VKPartialWindow(int width, int height, const char *title = "StoneMason Window (VULKAN)");

        VKPartialWindow(const VKPartialWindow &rhs) = delete;
        VKPartialWindow &operator=(const VKPartialWindow &rhs) = delete;

        VKPartialWindow(VKPartialWindow &&rhs) noexcept;
        VKPartialWindow &operator=(VKPartialWindow &&rhs) noexcept;
    };

    class VKWindow : public VKPartialWindow {
    public:
        vk::SurfaceKHR surface;
        VKDevice *pDev;
        vk::Extent2D swapExtent;

        vk::Format swapFmt = vk::Format::eB8G8R8A8Srgb;
        vk::ColorSpaceKHR swapColorSpace = vk::ColorSpaceKHR::eSrgbNonlinear;

        vk::SwapchainKHR swap;
        std::vector<vk::Image> swapImgs;
        std::vector<vk::ImageView> swapViews;

        friend class VKInstance;

    public:
        VKWindow() = delete;

        VKWindow(VKDevice *d, VKPartialWindow &&parent);
        ~VKWindow(); // dont add override

        VKWindow(const VKWindow &rhs) = delete;
        VKWindow &operator=(const VKWindow &rhs) = delete;

        VKWindow(VKWindow &&rhs) noexcept;
        VKWindow &operator=(VKWindow &&rhs) noexcept;
    };
}

#endif // STMS_ENABLE_VULKAN

#endif //__STONEMASON_VK_WINDOW_HPP
