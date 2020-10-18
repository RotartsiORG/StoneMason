//
// Created by grant on 7/5/20.
//
#include "stms/rend/vk/vk_pipeline.hpp"
#include "stms/stms.hpp"

int main() {
    stms::initAll();

    stms::VKInstance inst(stms::VKInstance::ConstructionDetails{"VK Demo", 0, 0, 0, {}, {}, true});
    auto gpus = inst.buildDeviceList();
    stms::VKDevice gpu(&inst, gpus[0], stms::VKDevice::ConstructionDetails({}, {}, {}, {}));

    stms::VKWindow win(&gpu, 640, 480, "StoneMason Vulkan Demo");

    stms::VKShader frag(&gpu, stms::readFile("./res/vkShaders/frag.spv"), vk::ShaderStageFlagBits::eFragment);
    stms::VKShader vert(&gpu, stms::readFile("./res/vkShaders/vert.spv"), vk::ShaderStageFlagBits::eVertex);

    while (!win.shouldClose()) {
        stms::pollEvents();
    }
}
