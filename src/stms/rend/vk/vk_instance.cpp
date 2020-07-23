//
// Created by grant on 7/23/20.
//

#include "stms/rend/vk/vk_instance.hpp"

#include <unordered_map>

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
}
