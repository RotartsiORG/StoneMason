//
// Created by grant on 7/5/20.
//
#include "stms/rend/vk/vk_pipeline.hpp"
#include "stms/rend/vk/vk_passes.hpp"
#include "stms/stms.hpp"
#include "stms/util/util.hpp"

int main() {
    stms::initAll();

    auto *preWin = new stms::VKPartialWindow{640, 480, "StoneMason Vulkan Demo"};

    stms::VKInstance inst(stms::VKInstance::ConstructionDetails{"DEMO", 0, 0, 1, {}, {}, true, stms::VKInstance::ValidateMode::eWhitelist, {{"VK_LAYER_KHRONOS_validation"}}});
    auto gpus = inst.buildDeviceList(preWin);
    stms::VKDevice gpu(&inst, gpus[0], stms::VKDevice::ConstructionDetails{});

    stms::VKWindow win(&gpu, std::move(*preWin));
    delete preWin; // preWin is safe to destroy at this point.

    stms::VKShader frag(&gpu, stms::readFile("./res/vkShaders/frag.spv"), vk::ShaderStageFlagBits::eFragment);
    stms::VKShader vert(&gpu, stms::readFile("./res/vkShaders/vert.spv"), vk::ShaderStageFlagBits::eVertex);

    stms::VKPipelineLayout layout{&win};

    vk::AttachmentDescription desc{{}, win.swapFmt, vk::SampleCountFlagBits::e1, vk::AttachmentLoadOp::eClear,
                                   vk::AttachmentStoreOp::eStore, vk::AttachmentLoadOp::eDontCare, vk::AttachmentStoreOp::eDontCare,
                                   vk::ImageLayout::eUndefined, vk::ImageLayout::ePresentSrcKHR};
    vk::AttachmentReference ref{0, vk::ImageLayout::eColorAttachmentOptimal};
    vk::SubpassDescription subDesc{{}, vk::PipelineBindPoint::eGraphics, 0, nullptr, 1, &ref, nullptr, nullptr, 0, nullptr};
    stms::VKRenderPass pass{&gpu, {{desc}}, {{subDesc}}, {}};

    stms::VKPipeline pipeline{&layout, &pass, 0, {{&frag, &vert}}, nullptr, {}};

    STMS_INFO("{} {} {} {}", (void*) pipeline.pipeline, (void*) frag.mod, (void*) vert.mod, (void*) pass.pass); // prevent stuff from being optimized out

    while (!win.shouldClose()) {
        stms::pollEvents();
    }
}
