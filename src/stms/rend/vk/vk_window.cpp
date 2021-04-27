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
    VKPartialWindow::VKPartialWindow(int width, int height, const char *title) {
        glfwDefaultWindowHints();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // no opengl api
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE); // TODO: swapchain recreation.
        win = glfwCreateWindow(width, height, title, nullptr, nullptr);

        if (!win) {
            STMS_ERROR("Failed to create vulkan window {}! Expect a crash!", title);
        }
    }

    VKPartialWindow::VKPartialWindow(VKPartialWindow &&rhs) noexcept {
        *this = std::move(rhs);
    }

    VKPartialWindow &VKPartialWindow::operator=(VKPartialWindow &&rhs) noexcept {
        if (this == &rhs || win == rhs.win) {
            return *this; // self assignment
        }

        win = rhs.win;
        rhs.win = nullptr;

        return *this;
    }


    VKWindow::VKWindow(VKDevice *d, VKPartialWindow &&parent) : pDev(d) {
        win = parent.win;
        parent.win = nullptr;

        int width, height;
        glfwGetWindowSize(win, &width, &height);

        VkSurfaceKHR rawSurf = VK_NULL_HANDLE;
        if (glfwCreateWindowSurface(d->pInst->inst, win, nullptr, &rawSurf) != VK_SUCCESS) {
            STMS_ERROR("Failed to create surface for window! Vulkan is unusable!");
        }
        surface = vk::SurfaceKHR(rawSurf);
        if (!d->phys.gpu.getSurfaceSupportKHR(d->phys.presentIndex, surface)) {
            STMS_FATAL("Present queue does not support this surface! This SHOULD be impossible!");
        }

        auto caps = d->phys.gpu.getSurfaceCapabilitiesKHR(surface);

        if (caps.currentExtent.width != UINT32_MAX) {
            swapExtent = caps.currentExtent;
        } else {
            swapExtent = vk::Extent2D{static_cast<uint32_t>(width), static_cast<uint32_t>(height)};

            swapExtent.width = std::max(caps.minImageExtent.width, std::min(caps.maxImageExtent.width, swapExtent.width));
            swapExtent.height = std::max(caps.minImageExtent.height, std::min(caps.maxImageExtent.height, swapExtent.height));
        }
        STMS_INFO("Swap extent is {}x{}", swapExtent.width, swapExtent.height);

        uint32_t imgCount = caps.maxImageCount >= caps.minImageCount ?
                            std::min(caps.minImageCount + 1, caps.maxImageCount) : caps.minImageCount + 1;

        STMS_INFO("Swap image count will be {} (max={}, min={})", imgCount, caps.maxImageCount, caps.minImageCount);


        auto fmts = d->phys.gpu.getSurfaceFormatsKHR(surface);
        for (const auto& i : fmts) {
            if (i.format == vk::Format::eB8G8R8A8Srgb && i.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
                STMS_INFO("Optimal format for swapchain supported! (non-linear SRGB, 4 channel RGBA)");
                goto skipSetFirst; // i'm sorry
            }
        }

        swapFmt = fmts[0].format;
        swapColorSpace = fmts[0].colorSpace;
        STMS_INFO("Settled for swap format of colorspace={}, format={}", swapColorSpace, swapFmt);

        skipSetFirst:

        bool queuesIdentical = d->phys.graphicsIndex == d->phys.presentIndex && d->phys.presentIndex == d->phys.computeIndex;
        if (queuesIdentical) {
            STMS_INFO("Using exclusive queue sharing mode as present, compute, and graphics queues are identical");
        } else {
            STMS_INFO("Using shared sharing mode as present, compute, and graphics queues are discrete");
        }

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
            pDev->device.destroyImageView(v);
        }

        pDev->device.destroySwapchainKHR(swap);
        pDev->pInst->inst.destroy(surface);
    }

    static inline const std::array<const char *, 55> &getDeviceFeatureList() {
        static constexpr auto val = std::array<const char *, 55>({"robustBufferAccess", "fullDrawIndexUint32", "imageCubeArray",
            "independentBlend", "geometryShader", "tessellationShader", "sampleRateShading", "dualSrcBlend", "logicOp",
            "multiDrawIndirect", "drawIndirectFirstInstance", "depthClamp", "depthBiasClamp", "fillModeNonSolid",
            "depthBounds", "wideLines", "largePoints", "alphaToOne", "multiViewport", "samplerAnisotropy",
            "textureCompressionETC2", "textureCompressionASTC_LDR", "textureCompressionBC", "occlusionQueryPrecise",
            "pipelineStatisticsQuery", "vertexPipelineStoresAndAtomics", "fragmentStoresAndAtomics",
            "shaderTessellationAndGeometryPointSize", "shaderImageGatherExtended", "shaderStorageImageExtendedFormats",
            "shaderStorageImageMultisample", "shaderStorageImageReadWithoutFormat", "shaderStorageImageWriteWithoutFormat",
            "shaderUniformBufferArrayDynamicIndexing", "shaderSampledImageArrayDynamicIndexing", "shaderStorageBufferArrayDynamicIndexing",
            "shaderStorageImageArrayDynamicIndexing", "shaderClipDistance", "shaderCullDistance", "shaderFloat64", "shaderInt64", "shaderInt16",
            "shaderResourceResidency", "shaderResourceMinLod", "sparseBinding", "sparseResidencyBuffer", "sparseResidencyImage2D",
            "sparseResidencyImage3D", "sparseResidency2Samples", "sparseResidency4Samples", "sparseResidency8Samples",
            "sparseResidency16Samples", "sparseResidencyAliased", "variableMultisampleRate", "inheritedQueries"});
        return val;
    }

    VKDevice::VKDevice(VKInstance *inst, VKGPU dev, const ConstructionDetails &details) : pInst(inst), phys(dev) {
        float prio = 1.0f;

        std::vector<const char *> deviceExts(details.requiredExts);

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
        for (const auto &i : deviceExts) {
            extDat[std::string(i)].required = true;
        }

        std::vector<vk::ExtensionProperties> supportedExts = dev.gpu.enumerateDeviceExtensionProperties();
        for (const auto &prop : supportedExts) {
            if (details.wantedExts.find(std::string(prop.extensionName)) != details.wantedExts.end()) {
                deviceExts.emplace_back(prop.extensionName);
            }

            extDat[std::string(prop.extensionName)].supported = true;
        }

        for (const auto &ext : deviceExts) {
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

            STMS_INFO("VkDeviceExt\t{:<42}\t\t{}", i.first, remark);
        }

        // Enable all required feats. If they aren't supported, it doesn't matter since they are REQUIRED.
        feats = details.requiredFeats;
        auto supportedFeats = dev.gpu.getFeatures();
        for (size_t i = 0; i < (sizeof(vk::PhysicalDeviceFeatures) / sizeof(vk::Bool32)); i++) {

            std::string name;
            try {
                name = getDeviceFeatureList().at(i);
            } catch (std::out_of_range &e) {
                name = "!OUT_OF_RANGE!";
            }

            bool isSupported = reinterpret_cast<vk::Bool32 *>(&supportedFeats)[i] == VK_TRUE;

            std::string remark = "\u001b[1m\u001b[31mDISABLED\u001b[0m"; // Bold Red
            if (isSupported) {
                remark += "\t\u001b[1m\u001b[32m!<< SUPPORTED!\u001b[0m"; // bold green
            }

            if (reinterpret_cast<vk::Bool32 *>(&feats)[i] == VK_TRUE) {
                remark = "\u001b[1m\u001b[93mREQUIRED\u001b[0m"; // bold magenta
                if (!isSupported) {
                    remark += "\t\u001b[1m\u001b[31m!<< NOT SUPPORTED! EXPECT CRASH!!\u001b[0m"; // bold red
                }
            } else if (reinterpret_cast<const vk::Bool32 *>(&details.wantedFeats)[i] == VK_TRUE) {
                remark = "\u001b[96mWANTED\u001b[0m";
                reinterpret_cast<vk::Bool32 *>(&feats)[i] = VK_TRUE;
                if (!isSupported) {
                    remark += "\t\u001b[1m\u001b[93m!<< NOT SUPPORTED! DISABLED!\u001b[0m"; // bold yellow
                    reinterpret_cast<vk::Bool32 *>(&feats)[i] = VK_FALSE;
                }
            }
            STMS_INFO("VkDevFeat#{}\t{:<42}\t{}", i, name, remark);
        }

        std::vector<vk::DeviceQueueCreateInfo> queues = {{{}, dev.graphicsIndex, 1, &prio}};
        if (dev.presentIndex != dev.graphicsIndex) {
            queues.emplace_back(vk::DeviceQueueCreateInfo{{}, dev.presentIndex, 1, &prio});
        }
        if (dev.computeIndex != dev.presentIndex && dev.computeIndex != dev.graphicsIndex) {
            queues.emplace_back(vk::DeviceQueueCreateInfo{{}, dev.computeIndex, 1, &prio});
        }

        vk::DeviceCreateInfo devCi{{}, static_cast<uint32_t>(queues.size()), queues.data(),
                                   static_cast<uint32_t>(inst->layers.size()), inst->layers.data(),
                                   static_cast<uint32_t>(deviceExts.size()), deviceExts.data(), &feats};

        device = dev.gpu.createDevice(devCi);

        device.getQueue(dev.graphicsIndex, 0, &graphics);
        device.getQueue(dev.presentIndex, 0, &present);
        device.getQueue(dev.computeIndex, 0, &compute);
    }

    VKDevice::~VKDevice() {
        device.destroy();
    }

    VKDevice::ConstructionDetails::ConstructionDetails(const vk::PhysicalDeviceFeatures &req,
                                                       const vk::PhysicalDeviceFeatures &want,
                                                       std::unordered_set<std::string> wantExt,
                                                       std::vector<const char *> reqExt)
            : requiredFeats(req), wantedFeats(want), wantedExts(std::move(wantExt)), requiredExts(std::move(reqExt)) {}
}

#endif // STMS_ENABLE_VULKAN