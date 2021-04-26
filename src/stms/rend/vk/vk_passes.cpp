#include "stms/rend/vk/vk_passes.hpp"

namespace stms {
    VKRenderPass::VKRenderPass(VKDevice *dev,
                    const std::vector<vk::AttachmentDescription> &attachments,
                    const std::vector<vk::SubpassDescription> &subpasses,
                    const std::vector<vk::SubpassDependency> &dependencies) : pDev(dev) {
        vk::RenderPassCreateInfo ci{{}, static_cast<uint32_t>(attachments.size()), attachments.data(), static_cast<uint32_t>(subpasses.size()),
                                    subpasses.data(), static_cast<uint32_t>(dependencies.size()), dependencies.data()};

        pass = dev->device.createRenderPass(ci);
    }

    VKRenderPass::~VKRenderPass() {
        pDev->device.destroyRenderPass(pass);
    }
}