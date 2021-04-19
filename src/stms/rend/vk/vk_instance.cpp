//
// Created by grant on 7/23/20.
//

#include "stms/rend/vk/vk_instance.hpp"
#ifdef STMS_ENABLE_VULKAN

#include <unordered_map>
#include <utility>

#include "stms/rend/vk/vk_window.hpp"

namespace stms {
    static bool devExtensionsSuitable(vk::PhysicalDevice dev, const std::vector<std::string> &required) {
        auto supportedExts = dev.enumerateDeviceExtensionProperties();

        std::unordered_set<std::string> supported;
        for (const auto &ext : supportedExts) {
            supported.emplace(std::string(ext.extensionName));
        }

        for (const auto &req : required) {
            if (supported.find(req) == supported.end()) {
                return false;
            }
        }
        return true;
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

        std::vector<vk::LayerProperties> layerProps;
        switch (details.validate) {
            case (ValidateMode::eBlacklist):
                STMS_INFO("Vulkan validation layers are enabled in blacklist mode!");
                exts.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
                layerProps = vk::enumerateInstanceLayerProperties();
                for (const auto &lyo : layerProps) {
                    if (details.layerList.find(std::string(lyo.layerName)) == details.layerList.end()) {
                        STMS_INFO("VkValidationLayer\t {:<32}\t\u001b[1m\u001b[32mENABLED\u001b[0m", lyo.layerName);
                        layers.emplace_back(lyo.layerName);
                    } else {
                        STMS_INFO("VkValidationLayer\t {:<32}\t\u001b[1m\u001b[31mFORBIDDEN\u001b[0m", lyo.layerName);
                    }
                }
                break;

            case (ValidateMode::eWhitelist):
                STMS_INFO("Vulkan validation layers are enabled in whitelist mode!");
                exts.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
                layerProps = vk::enumerateInstanceLayerProperties();
                for (const auto &lyo : layerProps) {
                    if (details.layerList.find(std::string(lyo.layerName)) != details.layerList.end()) {
                        STMS_INFO("VkValidationLayer\t {:<32}\t\u001b[1m\u001b[32mENABLED\u001b[0m", lyo.layerName);
                        layers.emplace_back(lyo.layerName);
                    } else {
                        STMS_INFO("VkValidationLayer\t {:<32}\t\u001b[1m\u001b[31mDISABLED\u001b[0m", lyo.layerName);
                    }
                }
                break;

            default:
                STMS_INFO("Vulkan validation layers are disabled!");
        }

        for (const auto &i : exts) {
            extDat[std::string(i)].required = true;
        }

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

        vk::InstanceCreateInfo ci{{}, &appInfo, static_cast<uint32_t>(layers.size()), layers.data(),
                                  static_cast<uint32_t>(exts.size()), exts.data()};


        vk::DebugUtilsMessageSeverityFlagsEXT severityBits =  vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose |
                                                              vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo |
                                                              vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
                                                              vk::DebugUtilsMessageSeverityFlagBitsEXT::eError;
        vk::DebugUtilsMessageTypeFlagsEXT typeBits = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
                                                     vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance |
                                                     vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation;

        // This MUST be outside of the if bc it will be read in vk::createInstance.
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

    std::vector<VKGPU> VKInstance::buildDeviceList(VKPartialWindow *win, const GPURequirements &specs) const {

        VkSurfaceKHR rawSurf = VK_NULL_HANDLE;
        if (glfwCreateWindowSurface(inst, win->win, nullptr, &rawSurf) != VK_SUCCESS) {
            STMS_ERROR("Failed to create dummy surface in VKInstance::buildDeviceList. Present queues will be empty.");
        }
        auto surface = vk::SurfaceKHR(rawSurf);

        std::vector<vk::PhysicalDevice> devs = inst.enumeratePhysicalDevices();

        std::vector<VKGPU> ret;
        ret.reserve(devs.size());

        for (const auto &d : devs) {
            auto props = d.getProperties();

            if (!devExtensionsSuitable(d, specs.requiredExtensions)) {
                STMS_WARN("Skipping card {} because it doesn't support the requisite extensions!", props.deviceName);
                continue; // Doesn't support swap-chain ext
            }

            if ((specs.needPresentModes && d.getSurfacePresentModesKHR(surface).empty()) ||
                (specs.needSurfaceFormats && d.getSurfaceFormatsKHR(surface).empty())) {
                STMS_WARN("Skipping card {} because its present modes and surface formats are inadequate!", props.deviceName);
                continue; // Swap chain is inadequate
            }

            VKGPU toInsert{};
            toInsert.gpu = d;

            auto queues = d.getQueueFamilyProperties();
            uint32_t i = 0;
            for (const auto &q : queues) {
                if (rawSurf && d.getSurfaceSupportKHR(i, surface)) {
                    toInsert.presentIndex = i;
                    toInsert.presentFound = true;
                }

                if (q.queueFlags & vk::QueueFlagBits::eGraphics) {
                    toInsert.graphicsFound = true;
                    toInsert.graphicsIndex = i;
                }

                if (q.queueFlags & vk::QueueFlagBits::eCompute) {
                    toInsert.computeIndex = i;
                    toInsert.computeFound = true;
                }

                if (toInsert.graphicsFound && toInsert.presentFound && toInsert.computeFound) {
                    break;
                }

                i++;
            }

            if ((toInsert.graphicsFound || !specs.needGraphicsQueue) &&
                (toInsert.presentFound || !specs.needPresentQueue) && 
                (toInsert.computeFound || !specs.needComputeQueue)) {
                ret.emplace_back(toInsert);
            } else {
                STMS_WARN("Skipping card {} because its queues are inadequate.", props.deviceName);
            }
        }

        inst.destroySurfaceKHR(surface);

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
        std::unordered_set<std::string> we, std::vector<const char *> re, ValidateMode v, std::unordered_set<std::string> ll) :
        appName(n), appMajor(ma), appMinor(mi), appMicro(mc), wantedExts(std::move(we)), requiredExts(std::move(re)),
        validate(v), layerList(ll) {}
}

#endif
