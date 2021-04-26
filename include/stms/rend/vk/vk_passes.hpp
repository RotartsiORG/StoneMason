#pragma once

#ifndef __STONEMASON_REND_VK_PASSESS_HPP
#define __STONEMASON_REND_VK_PASSESS_HPP

#include "stms/rend/vk/vk_window.hpp"

namespace stms {
    class VKRenderPass {
    public:
        vk::RenderPass pass;
        VKDevice *pDev;
    public:
        VKRenderPass(VKDevice *dev,
                     const std::vector<vk::AttachmentDescription> &attachments = {},
                     const std::vector<vk::SubpassDescription> &subpasses = {},
                     const std::vector<vk::SubpassDependency> &dependencies = {});
        VKRenderPass() = delete;

        virtual ~VKRenderPass();
    };
}

#endif