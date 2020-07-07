//
// Created by grant on 7/5/20.
//

#include "stms/rend/vk/vk_window.hpp"
#include "stms/stms.hpp"

int main() {
    stms::initAll();

    stms::VKWindow win(640, 480, "StoneMason Vulkan Demo");
    stms::VKInstance inst(stms::VKInstance::ConstructionDetails<0>{"VK Demo", 0, 0, 0, {}, {}, true});

    auto gpus = inst.buildDeviceList();
    auto name = gpus[0].gpu.getProperties().deviceName; // temp obj destroyed but it werks and i don't care enough!
    STMS_INFO("GOT GPU {}", name);

    while (!win.shouldClose()) {
        stms::pollEvents();
    }
}
