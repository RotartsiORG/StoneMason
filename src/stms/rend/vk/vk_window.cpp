//
// Created by grant on 7/5/20.
//

#include "stms/rend/vk/vk_window.hpp"
#ifdef STMS_ENABLE_VULKAN

#include <algorithm>
#include <vector>
#include <unordered_map>
#include <numeric>

namespace stms {

    static size_t getMemorySize(const vk::PhysicalDevice &dev, std::unordered_map<size_t, size_t> &sizeCache) {
        auto props = dev.getProperties();

        // Manually hash here bc so that we only compute hash once each cmp.
        size_t hash = (static_cast<size_t>(props.vendorID) << 32u) | props.deviceID;

        if (sizeCache.find(hash) != sizeCache.end()) {
            return sizeCache[hash];
        } else {
            vk::PhysicalDeviceMemoryProperties memProps = dev.getMemoryProperties();

            // 128 GB advantage for discrete GPUs.
            auto initScore = props.deviceType == vk::PhysicalDeviceType::eDiscreteGpu ? 1024 * 1024 * 1024 * 128 : 0;

            size_t ret = std::accumulate(memProps.memoryHeaps, memProps.memoryHeaps + memProps.memoryHeapCount, initScore,
                    [](size_t init, const vk::MemoryHeap& memHeap) {
                STMS_INFO("Heap Size: {}", memHeap.size);
                return init + memHeap.size;
            });

            STMS_INFO("Device {} has {} of VRAM", props.deviceName, ret);

            sizeCache[hash] = ret;
            return ret;
        }
    }

    VKWindow::VKWindow(int width, int height, const char *title) {
        glfwDefaultWindowHints();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // no opengl api
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE); // TODO: swapchain recreation.
        win = glfwCreateWindow(width, height, title, nullptr, nullptr);

        if (!win) {
            STMS_ERROR("Failed to create vulkan window {}! Expect a crash!", title);
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

    std::vector<VKGPU> VKInstance::buildDeviceList() {
        std::vector<vk::PhysicalDevice> devs = inst.enumeratePhysicalDevices();

        std::vector<VKGPU> ret;
        ret.reserve(devs.size());

        for (const auto &d : devs) {
            VKGPU toInsert{};
            toInsert.gpu = d;

            auto queues = d.getQueueFamilyProperties();
            bool graphicsFound = false;
            uint32_t i = 0;
            for (const auto &q : queues) {
                if (q.queueFlags & vk::QueueFlagBits::eGraphics) {
                    graphicsFound = true;
                    toInsert.graphicsIndex = i;
                    break;
                }

                i++;
            }

            if (graphicsFound) {
                ret.emplace_back(toInsert);
            }
        }

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

    VKDevice::VKDevice(VKInstance *inst, VKGPU dev, const vk::PhysicalDeviceFeatures& feats) {
        float prio = 1.0f;

        dev.gpu.enumerateDeviceExtensionProperties();

        vk::DeviceQueueCreateInfo graphicsCi{{}, dev.graphicsIndex, 1, &prio};
        vk::DeviceCreateInfo devCi{{}, 1, &graphicsCi, static_cast<uint32_t>(inst->layers.size()), inst->layers.data(), 0, nullptr, &feats};

        device = dev.gpu.createDevice(devCi);
    }

    VKDevice::~VKDevice() {
        device.destroy();
    }
}

#endif // STMS_ENABLE_VULKAN
