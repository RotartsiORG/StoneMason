//
// Created by grant on 7/5/20.
//

#include "stms/rend/vk/vk_window.hpp"
#include "stms/stms.hpp"

int main() {
    stms::initAll();

    stms::VKInstance inst(stms::VKInstance::ConstructionDetails<0>{"VK Demo", 0, 0, 0, {}, {}, true});
    auto gpus = inst.buildDeviceList();
    stms::VKDevice gpu(&inst, gpus[0], {});

    stms::VKWindow win(&gpu, 640, 480, "StoneMason Vulkan Demo");

    while (!win.shouldClose()) {
        stms::pollEvents();
    }
}
