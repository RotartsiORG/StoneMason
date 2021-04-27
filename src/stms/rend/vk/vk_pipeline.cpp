//
// Created by grant on 8/9/20.
//

#include "stms/rend/vk/vk_pipeline.hpp"

#ifdef STMS_ENABLE_VULKAN

namespace stms {

    VKShader::VKShader(VKDevice *dev, const std::vector<uint8_t> &bytecode, const vk::ShaderStageFlagBits &shaderStage,
                       const std::string &entryPoint, const VKShaderSpecialization &spec) : pDev(dev) {
        vk::ShaderModuleCreateInfo ci{{}, bytecode.size(), reinterpret_cast<const uint32_t *>(bytecode.data())};
        mod = dev->device.createShaderModule(ci);


        auto *name= new char[entryPoint.size()];
        strcpy(name, entryPoint.c_str());

        vk::SpecializationInfo vkSpec = vk::SpecializationInfo{static_cast<uint32_t>(spec.entries.size()), spec.entries.data(), 
                                                               static_cast<uint32_t>(spec.data.size()), spec.data.data()};

        bool useSpec = (!spec.data.empty()) && (!spec.entries.empty());
        stageCi = vk::PipelineShaderStageCreateInfo({}, shaderStage, mod, name, useSpec ? &vkSpec : nullptr);
    }

    VKShader::~VKShader() {
        delete[] stageCi.pName;
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

    VKPipelineCache::VKPipelineCache(VKDevice *gpu, const std::vector<uint8_t> &dat) : pDev(gpu) {
        vk::PipelineCacheCreateInfo ci{
            {}, static_cast<uint32_t>(dat.size()), dat.data()
        };

        cache = gpu->device.createPipelineCache(ci);
    }

    std::vector<uint8_t> VKPipelineCache::getData() {
        return pDev->device.getPipelineCacheData(cache);
    }

    vk::Result VKPipelineCache::mergeAndConsumeCaches(const std::vector<VKPipelineCache *> &caches) {
        auto *cacheHandles = new vk::PipelineCache[caches.size()];
        for (std::size_t i = 0; i < caches.size(); i++) {
            cacheHandles[i] = caches[i]->cache;
        }

        vk::Result res = pDev->device.mergePipelineCaches(cache, static_cast<uint32_t>(caches.size()), cacheHandles);
        delete[] cacheHandles;

        return res;
    }

    VKPipelineCache::~VKPipelineCache() {
        pDev->device.destroyPipelineCache(cache);
    }

    VKPipeline::VKPipeline(VKPipelineLayout *lyo, VKRenderPass *pass, uint32_t subpassIndex, const std::vector<VKShader *> &shaders, VKPipelineCache *cache, const VKPipelineConfig &config) : pDev(pass->pDev) {
        if (lyo->pWin->pDev != pass->pDev) {
            STMS_FATAL("VKPipeline is being constructed with mismatched devices! Expect errors!");
        }
        
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

        vk::PipelineVertexInputStateCreateInfo vertexInputCi{
            {}, static_cast<uint32_t>(vboBindingVec.size()), vboBindingVec.data(),
            static_cast<uint32_t>(vboAttribVec.size()), vboAttribVec.data()
        };

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
        auto *shaderStages = new vk::PipelineShaderStageCreateInfo[shaders.size()];
        for (std::size_t i = 0; i < shaders.size(); i++) {
            shaderStages[i] = shaders[i]->stageCi;
        }

        vk::GraphicsPipelineCreateInfo masterCi{{}, static_cast<uint32_t>(shaders.size()), shaderStages, &vertexInputCi, &config.inputAssemblyCi, 
        &config.tessellationCi, &viewStateCi, &config.rasterCi, &config.multiSampleCi, &config.depthStencilCi, &blendStateCi, &dynamicStateCi,
        lyo->layout, pass->pass, subpassIndex, vk::Pipeline{}, -1};

        // broken operator= appears to be a bug in vulkan.hpp
        pipeline = pass->pDev->device.createGraphicsPipeline(cache != nullptr ? cache->cache : nullptr, masterCi).value;
        delete[] shaderStages;
    }

    VKPipeline::~VKPipeline() {
        pDev->device.destroyPipeline(pipeline);
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
