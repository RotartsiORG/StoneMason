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
            size_t ret =  props.deviceType == vk::PhysicalDeviceType::eDiscreteGpu ? 1024UL * 1024UL * 1024UL * 128UL : 0;
            for (uint32_t i = 0; i < memProps.memoryHeapCount; i++) {
                ret += memProps.memoryHeaps[i].size;
            }

            STMS_INFO("Device {} has {} of VRAM", props.deviceName, ret);

            sizeCache[hash] = ret;
            return ret;
        }
    }

    static bool devExtensionsSuitable(vk::PhysicalDevice dev) {
        auto supportedExts = dev.enumerateDeviceExtensionProperties();
        for (const auto &ext : supportedExts) {
            if (std::strcmp(ext.extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0) {
                return true;
            }
        }
        return false;
    }

    VKWindow::VKWindow(VKDevice *d, int width, int height, const char *title) : parent(d) {
        glfwDefaultWindowHints();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // no opengl api
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE); // TODO: swapchain recreation.
        win = glfwCreateWindow(width, height, title, nullptr, nullptr);

        if (!win) {
            STMS_ERROR("Failed to create vulkan window {}! Expect a crash!", title);
        }

        VkSurfaceKHR rawSurf = VK_NULL_HANDLE;
        if (glfwCreateWindowSurface(d->parent->inst, win, nullptr, &rawSurf) != VK_SUCCESS) {
            STMS_ERROR("Failed to create surface for window {}! Vulkan is unusable!", title);
        }
        surface = vk::SurfaceKHR(rawSurf);

        // TODO: proper extent stuff & format & color space & img count selection!
        uint32_t imgCount = 1;
        vk::Format swapFmt = vk::Format::eB8G8R8A8Srgb;
        vk::ColorSpaceKHR swapColorSpace = vk::ColorSpaceKHR::eSrgbNonlinear;
        vk::Extent2D swapExtent;

        bool queuesIdentical = d->phys.graphicsIndex == d->phys.presentIndex;
        vk::SwapchainCreateInfoKHR swapCi{
                {}, surface, imgCount, swapFmt, swapColorSpace, swapExtent, 1, vk::ImageUsageFlagBits::eColorAttachment,
                queuesIdentical ? vk::SharingMode::eExclusive : vk::SharingMode::eConcurrent,
                queuesIdentical ? 1u : 2u, &d->phys.graphicsIndex, vk::SurfaceTransformFlagBitsKHR::eIdentity,
                vk::CompositeAlphaFlagBitsKHR::eOpaque, vk::PresentModeKHR::eFifo, VK_TRUE, vk::SwapchainKHR{}
        };

        swap = d->device.createSwapchainKHR(swapCi);

        swapImgs = d->device.getSwapchainImagesKHR(swap);

        swapViews.reserve(swapImgs.size());
        for (const auto &img : swapImgs) {
            vk::ImageViewCreateInfo ci{
                    {}, img, vk::ImageViewType::e2D, swapFmt, vk::ComponentMapping{},
                    vk::ImageSubresourceRange{vk::ImageAspectFlagBits::eColor, 0u, 1u, 0u, 1u}
            };

            swapViews.emplace_back(d->device.createImageView(ci));
        }
    }

    VKWindow::~VKWindow() {
        for (const auto &v : swapViews) {
            parent->device.destroyImageView(v);
        }

        parent->device.destroySwapchainKHR(swap);
        parent->parent->inst.destroy(surface);
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

    VKDevice::VKDevice(VKInstance *inst, VKGPU dev, const vk::PhysicalDeviceFeatures& feats) : parent(inst), phys(dev) {
        float prio = 1.0f;

        auto deviceExts = std::vector<const char *>({VK_KHR_SWAPCHAIN_EXTENSION_NAME});

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

    VKDevice::~VKDevice() {
        device.destroy();
    }
}

#endif // STMS_ENABLE_VULKAN
