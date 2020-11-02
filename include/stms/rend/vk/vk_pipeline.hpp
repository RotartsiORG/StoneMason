//
// Created by grant on 8/9/20.
//

#pragma once

#ifndef __STONEMASON_VK_PIPELINE_HPP
#define __STONEMASON_VK_PIPELINE_HPP
//!< Include guard

#ifdef STMS_ENABLE_VULKAN

#include <vector>
#include <string>

#include "stms/rend/vk/vk_window.hpp"

namespace stms {

    /// A vulkan shader! This object can be safely destroyed after it is used to create a pipeline.
    class VKShader {
    public:
        VKDevice *pDev{}; //!< Pointer to parent logical device
        vk::ShaderModule mod; //!< Internal implementation detail
        vk::PipelineShaderStageCreateInfo stageCi; //!< Internal implementation detail
    public:

        // is this true?
        // @throw If `stms::exceptionLevel > 0`, this will throw `std::runtime_error`
        //         *        if `glslSrc.size()` is not a multiple of 4.

        /**
         * @brief Constructor for VKShader
         * @param dev Logical device to use to create this VKShader
         * @param bytecode SPIR-V bytecode of the shader loaded from a file.
         * @param shaderStage The stage of the shader. See Vulkan documentation for `VkShaderStageFlagBits`
         * @param entryPoint Entry point of the shader, by default `main`.
         */
        VKShader(VKDevice *dev, const std::string &bytecode, const vk::ShaderStageFlagBits& shaderStage, const std::string &entryPoint = "main");
        virtual ~VKShader(); //!< Virtual destructor

        VKShader &operator=(const VKShader &rhs) = delete; //!< Deleted copy assignment operator
        VKShader(const VKShader &rhs) = delete; //!< Deleted copy constructor

        /**
         * @brief Move copy operator
         * @param rhs Right Hand side of `std::move`
         * @return Reference to `this`.
         */
        VKShader &operator=(VKShader &&rhs) noexcept;
        /**
         * @brief Move copy constructor
         * @param rhs Right Hand side of `std::move`
         */
        VKShader(VKShader &&rhs) noexcept;
    };

    struct VKPipelineConfig {
        std::vector<vk::VertexInputBindingDescription> vboBindingVec;
        std::vector<vk::VertexInputAttributeDescription> shaderAttribVec;

        vk::PipelineInputAssemblyStateCreateInfo inputAssemblyCi = vk::PipelineInputAssemblyStateCreateInfo{
            {}, // flags
            vk::PrimitiveTopology::eTriangleList, // topology
            VK_FALSE // primitiveRestartEnable
        };

        std::vector<vk::Viewport> viewportVec;
        std::vector<vk::Rect2D> scissorVec;

        vk::PipelineRasterizationStateCreateInfo rasterCi = vk::PipelineRasterizationStateCreateInfo{
            {}, // flags
            VK_FALSE, // depthClampEnable
            VK_FALSE, // rasterizerDiscardEnable
            vk::PolygonMode::eFill, // polygonMode
            vk::CullModeFlagBits::eNone, // cullMode
            vk::FrontFace::eCounterClockwise, // frontFace
            VK_FALSE, // depthBiasEnable
            0.0f, // depthBiasConstantFactor
            0.0f, // depthBiasClamp
            0.0f, // depthBiasSlopeFactor
            1.0f, // lineWidth
        };

        vk::PipelineMultisampleStateCreateInfo multiSampleCi = vk::PipelineMultisampleStateCreateInfo{
            {}, // flags
            vk::SampleCountFlagBits::e1, // rasterizationSamples
            VK_FALSE, // sampleShadingEnable
            1.0f, // minSampleShading
            nullptr, // pSampleMask
            VK_FALSE, // alphaToCoverageEnable
            VK_FALSE // alphaToOneEnable
        };

        vk::PipelineDepthStencilStateCreateInfo depthStencilCi = vk::PipelineDepthStencilStateCreateInfo{
                {}, // flags
                VK_FALSE, // depthTestEnable
                VK_FALSE, // depthWriteEnable
                vk::CompareOp::eNever, // depthCompareOp
                VK_FALSE, // depthBoundsTestEnable
                VK_FALSE, // stencilTestEnable
                vk::StencilOpState{
                    vk::StencilOp::eKeep, // failOp
                    vk::StencilOp::eKeep, // passOp
                    vk::StencilOp::eKeep, // depthFailOp
                    vk::CompareOp::eNever, // compareOp
                    0, // compareMask
                    0, // writeMask
                    0 // reference
                }, // front
                vk::StencilOpState{
                    vk::StencilOp::eKeep, // failOp
                    vk::StencilOp::eKeep, // passOp
                    vk::StencilOp::eKeep, // depthFailOp
                    vk::CompareOp::eNever, // compareOp
                    0, // compareMask
                    0, // writeMask
                    0 // reference
                }, // back
                0.0f, // minDepthBounds
                0.0f // maxDepthBounds
        };


        // Do these belong here? They are framebuffer attachments
        std::vector<vk::PipelineColorBlendAttachmentState> blendAttachVec;

//        vk::PipelineColorBlendStateCreateInfo blendCi = vk::PipelineColorBlendStateCreateInfo{
//                {},
//        };

        std::vector<vk::DynamicState> dynamicStateVec;

        std::vector<vk::DescriptorSetLayout> descSetLayoutVec;
        std::vector<vk::PushConstantRange> pushConstVec;
    };

    class VKPipeline {
    public:
        VKPipeline(VKWindow *win);

        VKPipeline(const VKPipeline &rhs) = delete;
        VKPipeline &operator=(const VKPipeline &rhs) = delete;

        VKPipeline(VKPipeline &&rhs) noexcept;
        VKPipeline &operator=(VKPipeline &&rhs) noexcept;


    };

    class VKVertexInput {

    };


}

#endif // STMS_ENABLE_VULKAN

#endif //__STONEMASON_VK_PIPELINE_HPP
