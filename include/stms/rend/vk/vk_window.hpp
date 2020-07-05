//
// Created by grant on 7/5/20.
//

#pragma once

#ifndef __STONEMASON_VK_WINDOW_HPP
#define __STONEMASON_VK_WINDOW_HPP

#include "stms/config.hpp"
#ifdef STMS_ENABLE_VULKAN

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <utility>
#include <vulkan/vulkan.hpp>

#include "stms/rend/window.hpp"
#include <array>
#include <unordered_set>
#include <stms/logging.hpp>

namespace stms {
    class VKWindow : public GenericWindow {
    public:
        VKWindow(int width, int height, const char *title="StoneMason Window (VULKAN)");
        ~VKWindow() override = default;

        VKWindow(const VKWindow &rhs) = delete;
        VKWindow &operator=(const VKWindow &rhs) = delete;

        VKWindow(VKWindow &&rhs) noexcept;
        VKWindow &operator=(VKWindow &&rhs) noexcept;
    };

    static std::unordered_set<std::string> &getForbiddenLayers() {
        static auto forbidden = std::unordered_set<std::string>({"VK_LAYER_LUNARG_vktrace"});
        return forbidden;
    }

    class VKInstance {
    private:
        vk::Instance inst;
        vk::DebugUtilsMessengerEXT debugMessenger;
        std::unordered_set<std::string> enabledExts;

    public:
        template <std::size_t numNeedExt>
        struct ConstructionDetails {
            /**
             * Simple struct constructor. Don't worry about the `std::move`s screwing up the objects you pass into
             * this, as they are passed in by value, not by reference.
             */
            ConstructionDetails(const char *n, short ma, short mi, short mc, std::unordered_set<std::string> we, std::array<const char *, numNeedExt> re, bool v) :
                appName(n), appMajor(ma), appMinor(mi), appMicro(mc), wantedExts(std::move(we)), requiredExts(std::move(re)), validate(v)
            {}

            const char *appName = "StoneMason Application (VULKAN)";

            short appMajor = 1;
            short appMinor = 0;
            short appMicro = 0;

            std::unordered_set<std::string> wantedExts;
            std::array<const char *, numNeedExt> requiredExts;

            bool validate = true;
        };

        template <std::size_t numReq>
        explicit VKInstance(const ConstructionDetails<numReq> &details);

        inline bool extEnabled(const std::string &ext) {
            return enabledExts.find(ext) != enabledExts.end();
        }

        virtual ~VKInstance();
    };



    static VKAPI_ATTR VkBool32 VKAPI_CALL vulkanDebugCallback(
            VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
            VkDebugUtilsMessageTypeFlagsEXT messageType,
            const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
            void* pUserData) {

        STMS_WARN("VK VALIDATION {}: {}", pCallbackData->pMessageIdName, pCallbackData->pMessage);

        return VK_FALSE;
    }

    template<std::size_t numReq>
    VKInstance::VKInstance(const VKInstance::ConstructionDetails<numReq> &details) {
        vk::ApplicationInfo appInfo{details.appName, static_cast<uint32_t>(VK_MAKE_VERSION(details.appMajor, details.appMinor, details.appMicro)),
                                    "StoneMason Engine", static_cast<uint32_t>(VK_MAKE_VERSION(versionMajor, versionMinor, versionMicro)),
                                    VK_VERSION_1_0};

        std::vector<const char *> exts(details.requiredExts.begin(), details.requiredExts.end());

        uint32_t glfwExtCount = 0;
        const char** glfwExts;

        glfwExts = glfwGetRequiredInstanceExtensions(&glfwExtCount);
        exts.insert(exts.end(), glfwExts, glfwExts + glfwExtCount);

        std::vector<vk::ExtensionProperties> extProps = vk::enumerateInstanceExtensionProperties();
        for (const auto &prop : extProps) {
            if (details.wantedExts.find(std::string(prop.extensionName)) != details.wantedExts.end()) {
                STMS_INFO("Enabled Vulkan extension {}", prop.extensionName);
                exts.push_back(prop.extensionName);
            } else {
                // not necessarily disabled, as it could be in details.requiredExts.
                STMS_INFO("Vulkan extension {} is available.", prop.extensionName);
            }
        }


        std::vector<const char *> layers;
        std::vector<vk::LayerProperties> layerProps;
        if (details.validate) {
            exts.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

            STMS_INFO("Vulkan validation layers are enabled!");
            layerProps = vk::enumerateInstanceLayerProperties();

            for (const auto &lyo : layerProps) {
                if (getForbiddenLayers().find(std::string(lyo.layerName)) == getForbiddenLayers().end()) {
                    STMS_INFO("Enabling validation layer {} ('{}')", lyo.layerName, lyo.description);
                    layers.emplace_back(lyo.layerName);
                } else {
                    STMS_INFO("IGNORING validation layer {} ('{}')", lyo.layerName, lyo.description);
                }
            }
        } else {
            STMS_INFO("Vulkan validation layers are disabled!");
        }

        for (const auto &ext : exts) {
            enabledExts.emplace(std::string(ext));
        }

        vk::InstanceCreateInfo ci{{}, &appInfo, static_cast<uint32_t>(layers.size()), layers.data(),
                                  static_cast<uint32_t>(exts.size()), exts.data()};


        vk::DebugUtilsMessageSeverityFlagsEXT severityBits =  vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose |
                                                              vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo |
                                                              vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
                                                              vk::DebugUtilsMessageSeverityFlagBitsEXT::eError;
        vk::DebugUtilsMessageTypeFlagsEXT typeBits = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
                                                     vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance |
                                                     vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation;

        vk::DebugUtilsMessengerCreateInfoEXT validationCbInfo{{}, severityBits, typeBits, vulkanDebugCallback, nullptr};

        if (details.validate && enabledExts.find(VK_EXT_DEBUG_UTILS_EXTENSION_NAME) != enabledExts.end()) {
            STMS_INFO("Setting up vulkan validation callback...");
            auto func = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
                    inst.getProcAddr("vkCreateDebugUtilsMessengerEXT"));

            if (func != nullptr) {
                auto rawCi = static_cast<VkDebugUtilsMessengerCreateInfoEXT>(validationCbInfo);
                auto rawMessenger = static_cast<VkDebugUtilsMessengerEXT>(debugMessenger);

                func(inst, &rawCi, nullptr, &rawMessenger);
            } else {
                STMS_INFO("Failed to load 'vkCreateDebugUtilsMessengerEXT' despite extension being present!");
            }

            ci.pNext = &validationCbInfo;
        }

        inst = vk::createInstance(ci);

    }
}

#endif // STMS_ENABLE_VULKAN

#endif //__STONEMASON_VK_WINDOW_HPP
