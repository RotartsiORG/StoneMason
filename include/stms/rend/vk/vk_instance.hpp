//
// Created by grant on 7/23/20.
//

#pragma once

#ifndef __STONEMASON_VK_INSTANCE_HPP
#define __STONEMASON_VK_INSTANCE_HPP

#ifdef STMS_ENABLE_VULKAN

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <unordered_set>
#include <vulkan/vulkan.hpp>

#include "stms/logging.hpp"

namespace stms {
    struct VKGPU {
        vk::PhysicalDevice gpu;
        uint32_t graphicsIndex{};
        uint32_t presentIndex{};
        uint32_t computeIndex{};

        bool graphicsFound = false;
        bool presentFound = false;
        bool computeFound = false;
    };

    class VKPartialWindow;

    struct GPURequirements {
        bool needPresentQueue = true;
        bool needGraphicsQueue = true;
        bool needComputeQueue = true;

        bool needPresentModes = true;
        bool needSurfaceFormats = true;

        std::vector<std::string> requiredExtensions = {{VK_KHR_SWAPCHAIN_EXTENSION_NAME}};
    };

    class VKInstance {
    public:
        vk::Instance inst;
        vk::DebugUtilsMessengerEXT debugMessenger;
        std::unordered_set<std::string> enabledExts;

        std::vector<const char *> layers;

    public:
        enum ValidateMode {
            eDontValidate = 0,
            eBlacklist = 1,
            eWhitelist = 2
        };

        struct ConstructionDetails {
            /**
             * Simple struct constructor. Don't worry about the `std::move`s screwing up the objects you pass into
             * this, as they are passed in by value, not by reference.
             */
            ConstructionDetails(const char *n, short ma, short mi, short mc, std::unordered_set<std::string> we,
                                std::vector<const char *> re, bool rg, ValidateMode v, std::unordered_set<std::string> ll);

            ConstructionDetails() = default;

            const char *appName = "StoneMason Application (VULKAN)";

            short appMajor = 1;
            short appMinor = 0;
            short appMicro = 0;

            std::unordered_set<std::string> wantedExts;
            std::vector<const char *> requiredExts;

            bool requireGlfwExts = true;

            ValidateMode validate = eBlacklist;
            std::unordered_set<std::string> layerList = {{"VK_LAYER_LUNARG_vktrace", "VK_LAYER_LUNARG_api_dump", "VK_LAYER_LUNARG_demo_layer",
                 "VK_LAYER_LUNARG_monitor", "VK_LAYER_VALVE_steam_fossilize_32", "VK_LAYER_VALVE_steam_fossilize_64",
                 "VK_LAYER_LUNARG_device_simulation", "VK_LAYER_MANGOHUD_overlay", "VK_LAYER_VKBASALT_post_processing", "VK_LAYER_LUNARG_gfxreconstruct",
                 "VK_LAYER_LUNARG_screenshot"}};
        };

        explicit VKInstance(const ConstructionDetails &details);

        inline bool extEnabled(const std::string &ext) {
            return enabledExts.find(ext) != enabledExts.end();
        }

        // In the future, the client would be allowed to pass in suitability parameters
        // to decide which devices are suitable.
        std::vector<VKGPU> buildDeviceList(VKPartialWindow *win = nullptr, const GPURequirements &specs = {}) const;

        virtual ~VKInstance();
    };
}

#endif
#endif //__STONEMASON_VK_INSTANCE_HPP
