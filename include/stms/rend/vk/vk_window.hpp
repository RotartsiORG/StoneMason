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

        template<unsigned numNeedExt>
        struct ConstructionDetails {
            ConstructionDetails(const vk::PhysicalDeviceFeatures &req, const vk::PhysicalDeviceFeatures &want,
                    std::unordered_set<std::string> wantExt, const std::array<const char *, numNeedExt> reqExt)
                    : requiredFeats(req), wantedFeats(want), wantedExts(std::move(wantExt)), requiredExts(reqExt) {};

            vk::PhysicalDeviceFeatures requiredFeats;
            vk::PhysicalDeviceFeatures wantedFeats;

            std::unordered_set<std::string> wantedExts;
            std::array<const char *, numNeedExt> requiredExts;
        };

        template <unsigned n>
        VKDevice(VKInstance *inst, VKGPU dev, const ConstructionDetails<n>& details = {});
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



    template <unsigned n>
    VKDevice::VKDevice(VKInstance *inst, VKGPU dev, const ConstructionDetails<n> &details) : pInst(inst), phys(dev) {
        float prio = 1.0f;

        auto deviceExts = std::vector<const char *>(details.requiredExts.begin(), details.requiredExts.end());
        deviceExts.emplace_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME); // required ext :/

        std::vector<vk::ExtensionProperties> supportedExts = dev.gpu.enumerateDeviceExtensionProperties();
        for (const auto &prop : supportedExts) {
            if (details.wantedExts.find(std::string(prop.extensionName)) != details.wantedExts.end()) {
                deviceExts.emplace_back(prop.extensionName);
            }
        }

        for (const auto &ext : deviceExts) {
            enabledExts.emplace(std::string(ext));
            STMS_INFO("Enabled Device extension: \t{}", ext);
        }

        feats = details.requiredFeats;
        auto supportedFeats = dev.gpu.getFeatures();
        for (size_t i = 0; i < (sizeof(vk::PhysicalDeviceFeatures) / sizeof(vk::Bool32)); i++) {
            if (reinterpret_cast<vk::Bool32 *>(&supportedFeats)[i] == VK_TRUE &&
                    reinterpret_cast<const vk::Bool32 *>(&details.wantedFeats)[i] == VK_TRUE) {
                reinterpret_cast<vk::Bool32 *>(&feats)[i] = VK_TRUE;
                STMS_INFO("Device Feature {}: \t\tENABLED", i);
            } else if (reinterpret_cast<vk::Bool32 *>(&feats)[i] == VK_TRUE) {
                STMS_INFO("Device Feature {}: \t\tENABLED", i);
            } else {
                STMS_INFO("Device Feature {}: \t\tDISABLED", i);
            }
        }

        std::vector<vk::DeviceQueueCreateInfo> queues = {{{}, dev.graphicsIndex, 1, &prio}};
        if (dev.presentIndex != dev.graphicsIndex) {
            queues.emplace_back(vk::DeviceQueueCreateInfo{{}, dev.presentIndex, 1, &prio});
        }

        vk::DeviceCreateInfo devCi{{}, static_cast<uint32_t>(queues.size()), queues.data(),
                                   static_cast<uint32_t>(inst->layers.size()), inst->layers.data(),
                                   static_cast<uint32_t>(deviceExts.size()), deviceExts.data(), &feats};

        device = dev.gpu.createDevice(devCi);

        device.getQueue(dev.graphicsIndex, 0, &graphics);
        device.getQueue(dev.presentIndex, 0, &present);
    }
}

#endif // STMS_ENABLE_VULKAN

#endif //__STONEMASON_VK_WINDOW_HPP
