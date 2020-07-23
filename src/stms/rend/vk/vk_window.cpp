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
    VKWindow::VKWindow(VKDevice *d, int width, int height, const char *title) : pDev(d) {
        glfwDefaultWindowHints();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // no opengl api
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE); // TODO: swapchain recreation.
        win = glfwCreateWindow(width, height, title, nullptr, nullptr);

        if (!win) {
            STMS_ERROR("Failed to create vulkan window {}! Expect a crash!", title);
        }

        VkSurfaceKHR rawSurf = VK_NULL_HANDLE;
        if (glfwCreateWindowSurface(d->pInst->inst, win, nullptr, &rawSurf) != VK_SUCCESS) {
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
            pDev->device.destroyImageView(v);
        }

        pDev->device.destroySwapchainKHR(swap);
        pDev->pInst->inst.destroy(surface);
    }

    VKDevice::~VKDevice() {
        device.destroy();
    }
}

#endif // STMS_ENABLE_VULKAN
