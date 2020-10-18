//
// Created by grant on 7/23/20.
//

#include "stms/rend/vk/vk_instance.hpp"
#ifdef STMS_ENABLE_VULKAN

#include <unordered_map>
#include <utility>

namespace stms {
    static bool devExtensionsSuitable(vk::PhysicalDevice dev) {
        auto supportedExts = dev.enumerateDeviceExtensionProperties();
        for (const auto &ext : supportedExts) {
            if (std::strcmp(ext.extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0) {
                return true;
            }
        }
        return false;
    }

    static size_t getMemorySize(const vk::PhysicalDevice &dev, std::unordered_map<size_t, size_t> &sizeCache) {
        auto props = dev.getProperties();

        // Manually hash here bc so that we only compute hash once each cmp.
        size_t hash = (static_cast<size_t>(props.vendorID) << 32u) | props.deviceID;

        if (sizeCache.find(hash) != sizeCache.end()) {
            return sizeCache[hash];
        } else {
            vk::PhysicalDeviceMemoryProperties memProps = dev.getMemoryProperties();

            // 128 GB advantage for discrete GPUs.
            size_t ret =  props.deviceType == vk::PhysicalDeviceType::eDiscreteGpu ? 1024UL * 1024UL * 1024UL * 128UL : 0;
            for (uint32_t i = 0; i < memProps.memoryHeapCount; i++) {
                ret += memProps.memoryHeaps[i].size;
            }

            STMS_INFO("Device {} has {} of VRAM", props.deviceName, ret);

            sizeCache[hash] = ret;
            return ret;
        }
    }

    static VKAPI_ATTR VkBool32 VKAPI_CALL vulkanDebugCallback(
            VkDebugUtilsMessageSeverityFlagBitsEXT,
            VkDebugUtilsMessageTypeFlagsEXT,
            const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
            void*) {

        if (pCallbackData->pMessageIdName != nullptr) {
            // Ignore loader messages bc they are just noise
            if (std::strcmp(pCallbackData->pMessageIdName, "Loader Message") == 0) {
                // STMS_TRACE("{}", pCallbackData->pMessage);
                return VK_FALSE;
            }
        }

        STMS_WARN("{}", pCallbackData->pMessage);

        return VK_FALSE;
    }

    static std::unordered_set<std::string> &getForbiddenLayers() {
        static auto forbidden = std::unordered_set<std::string>(
                {"VK_LAYER_LUNARG_vktrace", "VK_LAYER_LUNARG_api_dump", "VK_LAYER_LUNARG_demo_layer",
                 "VK_LAYER_LUNARG_monitor", "VK_LAYER_VALVE_steam_fossilize_32", "VK_LAYER_VALVE_steam_fossilize_64"});
        return forbidden;
    }

    VKInstance::VKInstance(const VKInstance::ConstructionDetails &details) {
        vk::ApplicationInfo appInfo{details.appName, static_cast<uint32_t>(VK_MAKE_VERSION(details.appMajor, details.appMinor, details.appMicro)),
                                    "StoneMason Engine", static_cast<uint32_t>(VK_MAKE_VERSION(versionMajor, versionMinor, versionMicro)),
                                    VK_API_VERSION_1_0};

        std::vector<const char *> exts(details.requiredExts);

        struct _tmp_ExtDat {
            bool required = false;
            bool supported = false;
            bool wanted = false;
            bool enabled = false;
        };
        std::unordered_map<std::string, _tmp_ExtDat> extDat;
        for (const auto &i : details.wantedExts) {
            extDat[i].wanted = true;
        }

        uint32_t glfwExtCount = 0;
        const char** glfwExts = glfwGetRequiredInstanceExtensions(&glfwExtCount);
        exts.insert(exts.end(), glfwExts, glfwExts + glfwExtCount);

        for (const auto &i : exts) {
            extDat[std::string(i)].required = true;
        }

        // TODO: If there is an extension in `wantedExts` that isn't available, that is never logged!
        std::vector<vk::ExtensionProperties> extProps = vk::enumerateInstanceExtensionProperties();
        for (const auto &prop : extProps) {
            if (details.wantedExts.find(std::string(prop.extensionName)) != details.wantedExts.end()) {
                exts.push_back(prop.extensionName);
            }

            extDat[std::string(prop.extensionName)].supported = true;
        }

        for (const auto &ext : exts) {
            std::string s(ext);
            extDat[s].enabled = true;
            enabledExts.emplace(s);
        }

        for (const auto &i : extDat) {
            std::string remark = "\u001b[1m\u001b[31mDISABLED\u001b[0m";
            if (i.second.required || i.second.enabled) {
                remark = i.second.required ? "\u001b[93mREQUIRED\u001b[0m" : "\u001b[1m\u001b[32mENABLED\u001b[0m";
                if (!i.second.supported) {
                    remark += "\t\u001b[1m\u001b[31m!<< NOT SUPPORTED! EXPECT CRASH!!\u001b[0m";
                }
            } else if (i.second.wanted) {
                remark = "\u001b[96mWANTED\u001b[0m";
                if (!i.second.supported) {
                    remark += "\t\u001b[1m\u001b[93m!<< NOT SUPPORTED! DISABLED!\u001b[0m";
                }
            }

            STMS_INFO("VkExt\t{:<42}\t\t{}", i.first, remark);
        }


        std::vector<vk::LayerProperties> layerProps;
        if (details.validate) {
            STMS_INFO("Vulkan validation layers are enabled!");
            exts.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
            layerProps = vk::enumerateInstanceLayerProperties();
            for (const auto &lyo : layerProps) {
                if (getForbiddenLayers().find(std::string(lyo.layerName)) == getForbiddenLayers().end()) {
                    STMS_INFO("VkValidationLayer\t {:<32}\t\u001b[1m\u001b[32mENABLED\u001b[0m", lyo.layerName);
                    layers.emplace_back(lyo.layerName);
                } else {
                    STMS_INFO("VkValidationLayer\t {:<32}\t\u001b[1m\u001b[31mFORBIDDEN\u001b[0m", lyo.layerName);
                }
            }
        } else {
            STMS_INFO("Vulkan validation layers are disabled!");
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
                STMS_ERROR("Failed to load 'vkCreateDebugUtilsMessengerEXT' despite extension being present!");
            }

            ci.pNext = &validationCbInfo;
        }

        inst = vk::createInstance(ci);

    }

    VKInstance::~VKInstance() {
        if (debugMessenger != vk::DebugUtilsMessengerEXT()) { // not null handle?
            auto func = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(inst.getProcAddr("vkDestroyDebugUtilsMessengerEXT"));
            if (func != nullptr) {

                func(inst, debugMessenger, nullptr);
            }
        }

        inst.destroy();
    }

    std::vector<VKGPU> VKInstance::buildDeviceList() const {
        glfwDefaultWindowHints();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // no opengl api
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
        GLFWwindow *win = glfwCreateWindow(8, 8, "Dummy Present Target", nullptr, nullptr);

        VkSurfaceKHR rawSurf;
        if (glfwCreateWindowSurface(inst, win, nullptr, &rawSurf) != VK_SUCCESS) {
            STMS_ERROR("Failed to create dummy surface target! Returning empty vector from VKInstance::buildDeviceList()!");
            return {};
        }
        auto surface = vk::SurfaceKHR(rawSurf);

        std::vector<vk::PhysicalDevice> devs = inst.enumeratePhysicalDevices();

        std::vector<VKGPU> ret;
        ret.reserve(devs.size());

        for (const auto &d : devs) {
            auto props = d.getProperties();

            if (!devExtensionsSuitable(d)) {
                STMS_WARN("Skipping card {} because it doesn't support the swapchain extension!", props.deviceName);
                continue; // Doesn't support swap-chain ext
            }

            if (d.getSurfacePresentModesKHR(surface).empty() || d.getSurfaceFormatsKHR(surface).empty()) {
                STMS_WARN("Skipping card {} because its present modes and surface formats are inadequate!", props.deviceName);
                continue; // Swap chain is inadequate
            }

            VKGPU toInsert{};
            toInsert.gpu = d;

            auto queues = d.getQueueFamilyProperties();
            bool graphicsFound = false;
            bool presentFound = false;
            uint32_t i = 0;
            for (const auto &q : queues) {
                if (d.getSurfaceSupportKHR(i, surface)) {
                    toInsert.presentIndex = i;
                    presentFound = true;
                }

                if (q.queueFlags & vk::QueueFlagBits::eGraphics) {
                    graphicsFound = true;
                    toInsert.graphicsIndex = i;
                    break;
                }

                i++;
            }

            if (graphicsFound && presentFound) {
                ret.emplace_back(toInsert);
            } else {
                STMS_WARN("Skipping card {} because its queues are inadequate.", props.deviceName);
            }
        }

        inst.destroySurfaceKHR(surface);
        glfwDestroyWindow(win);

        std::unordered_map<size_t, size_t> sizeCache;

        auto func = [&](VKGPU i, VKGPU j) -> bool {
            auto jSize = getMemorySize(j.gpu, sizeCache);
            auto iSize = getMemorySize(i.gpu, sizeCache);

            STMS_INFO("CMP {} {}", iSize, jSize);

            return iSize < jSize;
        };

        std::sort(ret.begin(), ret.end(), func);

        return ret;
    }

    VKInstance::ConstructionDetails::ConstructionDetails(const char *n, short ma, short mi, short mc,
        std::unordered_set<std::string> we, std::vector<const char *> re, bool v) :
        appName(n), appMajor(ma), appMinor(mi), appMicro(mc), wantedExts(std::move(we)), requiredExts(std::move(re)),
        validate(v) {}
}

#endif
