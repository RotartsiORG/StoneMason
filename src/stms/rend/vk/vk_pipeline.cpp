//
// Created by grant on 8/9/20.
//

#include "stms/rend/vk/vk_pipeline.hpp"

#ifdef STMS_ENABLE_VULKAN

namespace stms {

    VKShader::VKShader(VKDevice *dev, const std::string &bytecode, const vk::ShaderStageFlagBits &shaderStage,
                       const std::string &entryPoint) : pDev(dev) {
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

    VKPipeline::VKPipeline(VKPipelineLayout *lyo, const VKPipelineConfig &config) : pLayout(lyo) {

        std::vector<vk::VertexInputBindingDescription> vboBindingVec;
        std::vector<vk::VertexInputAttributeDescription> vboAttribVec;

        uint32_t bindingCounter = 0;
        for (const auto &row : config.vboLayout) {
            for (const auto &attrib : row.attribs) {
                vboAttribVec.emplace_back(vk::VertexInputAttributeDescription{attrib.location, bindingCounter,
                                                                              attrib.format, attrib.offset});
            }
            vboBindingVec.emplace_back(vk::VertexInputBindingDescription{bindingCounter++, row.stride, row.rate});
        }

        std::vector<vk::Viewport> viewportVec = config.viewportVec;
        viewportVec.insert(viewportVec.begin(),
                           vk::Viewport{0, 0, static_cast<float>(lyo->pWin->swapExtent.width),
                                        static_cast<float>(lyo->pWin->swapExtent.height), 0.0f, 1.0f});

        std::vector<vk::Rect2D> scissorVec = config.scissorVec;
        scissorVec.insert(scissorVec.begin(), vk::Rect2D{vk::Offset2D{0, 0}, lyo->pWin->swapExtent});

        vk::PipelineViewportStateCreateInfo viewStateCi{
                {}, static_cast<uint32_t>(viewportVec.size()), viewportVec.data(),
                static_cast<uint32_t>(scissorVec.size()), scissorVec.data()
        };

        std::vector<vk::PipelineColorBlendAttachmentState> blendAttachVec = config.blendAttachVec;
        blendAttachVec.insert(blendAttachVec.begin(), vk::PipelineColorBlendAttachmentState{
                VK_TRUE, vk::BlendFactor::eSrcAlpha, vk::BlendFactor::eOneMinusSrcAlpha, vk::BlendOp::eAdd,
                vk::BlendFactor::eOne, vk::BlendFactor::eZero, vk::BlendOp::eAdd,
                vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB |
                vk::ColorComponentFlagBits::eA
        });

        vk::PipelineColorBlendStateCreateInfo blendStateCi{
                {}, config.blendState.enable, config.blendState.logicOp,
                static_cast<uint32_t>(blendAttachVec.size()), blendAttachVec.data(),
                config.blendState.blendConstants
        };

        // config.dynamicStateVec = {};
        vk::PipelineDynamicStateCreateInfo dynamicStateCi{
                {}, static_cast<uint32_t>(config.dynamicStateVec.size()), config.dynamicStateVec.data()
        };

        // todo: create the fricking pipeline lol

    }

    VKPipeline::~VKPipeline() {
        // tb
    }

    VKPipelineLayout::VKPipelineLayout(VKWindow *win, const std::vector<vk::DescriptorSetLayout> &descSetLayoutVec,
                                       const std::vector<vk::PushConstantRange> &pushConstVec) : pWin(win) {
        vk::PipelineLayoutCreateInfo layoutCi{
                {}, static_cast<uint32_t>(descSetLayoutVec.size()), descSetLayoutVec.data(),
                static_cast<uint32_t>(pushConstVec.size()), pushConstVec.data()
        };

        layout = win->pDev->device.createPipelineLayout(layoutCi);
    }

    VKPipelineLayout::~VKPipelineLayout() {
        pWin->pDev->device.destroyPipelineLayout(layout);
    }
}

#endif
