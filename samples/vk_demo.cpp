//
// Created by grant on 7/5/20.
//
#include "stms/rend/vk/vk_pipeline.hpp"
#include "stms/stms.hpp"
#include "stms/util/util.hpp"

int main() {
    stms::initAll();

    auto *preWin = new stms::VKPartialWindow{640, 480, "StoneMason Vulkan Demo"};

    stms::VKInstance inst(stms::VKInstance::ConstructionDetails{"VK Demo", 0, 0, 0, {}, {}, true});
    auto gpus = inst.buildDeviceList(preWin);
    stms::VKDevice gpu(&inst, gpus[0], stms::VKDevice::ConstructionDetails({}, {}, {}, {}));

    stms::VKWindow win(&gpu, std::move(*preWin));
    delete preWin; // preWin is safe to destroy at this point.

    stms::VKShader frag(&gpu, stms::readFile("./res/vkShaders/frag.spv"), vk::ShaderStageFlagBits::eFragment);
    stms::VKShader vert(&gpu, stms::readFile("./res/vkShaders/vert.spv"), vk::ShaderStageFlagBits::eVertex);

    stms::VKPipelineLayout layout{&win};

    stms::VKPipeline pipeline{&layout};

    STMS_INFO("{} {} {}", (void*) pipeline.pLayout, (void*) frag.mod, (void*) vert.mod); // prevent stuff from being optimized out

    while (!win.shouldClose()) {
        stms::pollEvents();
    }
}
