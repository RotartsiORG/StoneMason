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

        VKInstance *pInst{};

        VKGPU phys;

        vk::PhysicalDeviceFeatures feats;
        std::unordered_set<std::string> enabledExts;

    public:

        struct ConstructionDetails {
            ConstructionDetails() = default;

            ConstructionDetails(const vk::PhysicalDeviceFeatures &req, const vk::PhysicalDeviceFeatures &want,
                    std::unordered_set<std::string> wantExt, std::vector<const char *> reqExt);

            vk::PhysicalDeviceFeatures requiredFeats;
            vk::PhysicalDeviceFeatures wantedFeats;

            std::unordered_set<std::string> wantedExts;
            std::vector<const char *> requiredExts;
        };

        VKDevice(VKInstance *inst, VKGPU dev, const ConstructionDetails& details = {});
        virtual ~VKDevice();

        inline bool extEnabled(const std::string &ext) {
            return enabledExts.find(ext) != enabledExts.end();
        }
    };

    class VKWindow : public GenericWindow {
    public:
        vk::SurfaceKHR surface;
        VKDevice *pDev;

        vk::SwapchainKHR swap;
        std::vector<vk::Image> swapImgs;
        std::vector<vk::ImageView> swapViews;

        friend class VKInstance;

    public:
        VKWindow(VKDevice *d, int width, int height, const char *title="StoneMason Window (VULKAN)");
        ~VKWindow() override;

        VKWindow(const VKWindow &rhs) = delete;
        VKWindow &operator=(const VKWindow &rhs) = delete;

        VKWindow(VKWindow &&rhs) noexcept;
        VKWindow &operator=(VKWindow &&rhs) noexcept;
    };
}

#endif // STMS_ENABLE_VULKAN

#endif //__STONEMASON_VK_WINDOW_HPP
