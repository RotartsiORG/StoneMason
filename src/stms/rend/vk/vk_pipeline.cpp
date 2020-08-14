//
// Created by grant on 8/9/20.
//

#include "stms/rend/vk/vk_pipeline.hpp"

#ifdef STMS_ENABLE_VULKAN

#include "stms/config.hpp"

namespace stms {
    VKShader::VKShader(VKDevice *dev, const std::string &bytecode, const VKShaderStageBits& shaderStage, const std::string &entryPoint) : pDev(dev) {
        vk::ShaderModuleCreateInfo ci{{}, bytecode.size(), reinterpret_cast<const uint32_t *>(bytecode.data())};
        mod = dev->device.createShaderModule(ci);

        stageCi = vk::PipelineShaderStageCreateInfo({}, shaderStage, mod, entryPoint.c_str(), nullptr);
    }

    VKShader::~VKShader() {
        pDev->device.destroyShaderModule(mod);
    }

    VKShader &VKShader::operator=(VKShader &&rhs) noexcept {
        if (this == &rhs || stageCi == rhs.stageCi) {
            return *this;
        }

        stageCi = rhs.stageCi;
        mod = rhs.mod;

        rhs.mod = vk::ShaderModule();

        return *this;
    }

    VKShader::VKShader(VKShader &&rhs) noexcept {
        *this = std::move(rhs);
    }
}

#endif
