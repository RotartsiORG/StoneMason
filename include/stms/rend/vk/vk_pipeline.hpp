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

#include "stms/rend/vk/vk_passes.hpp"

namespace stms {

    struct VKShaderSpecialization {
        typedef vk::SpecializationMapEntry Entry;

        std::vector<Entry> entries;
        std::vector<uint8_t> data;
    };

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

        VKShader(VKDevice *dev, const std::vector<uint8_t> &bytecode, const vk::ShaderStageFlagBits& shaderStage, const std::string &entryPoint = "main", const VKShaderSpecialization &spec = {});
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
        /**
         * @brief Layout of a vertex buffer, represents a buffer to load data from.
         */
        struct VBOLayout {
            vk::VertexInputRate rate = vk::VertexInputRate::eVertex; //!< How often we move on to the next 'vertex'. For instancing.
            uint32_t stride; //!< Number of bytes each whole vertex takes. Allows for implicit padding.

            /**
             * @brief Represents a vertex attribute in a VBO.
             */
            struct AttribLayout {
                /**
                 * Shader location number (i.e. if location was 123, then this attrib could be accessed in the shader with
                 * `layout(location = 123) vec4 color;`. Note that some types [arrays, dvec4s, etc] take multiple locations)
                 */
                uint32_t location;

                uint32_t offset; //!< Offset from beginning of buffer in bytes.
                vk::Format format; //!< Format of data, see vulkan docs.
            };

            std::vector<AttribLayout> attribs; //!< List of `VkVertexAttribLayout`s for specific vertex attribs.
        };

        std::vector<VBOLayout> vboLayout;

        vk::PipelineInputAssemblyStateCreateInfo inputAssemblyCi = vk::PipelineInputAssemblyStateCreateInfo{
            {}, // flags
            vk::PrimitiveTopology::eTriangleList, // topology
            VK_FALSE // primitiveRestartEnable
        };

        vk::PipelineTessellationStateCreateInfo tessellationCi = vk::PipelineTessellationStateCreateInfo{{}, 0};

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

        struct BlendState {
            vk::Bool32 enable = VK_FALSE;
            vk::LogicOp logicOp = vk::LogicOp::eCopy;
            std::array<float, 4> blendConstants = std::array<float, 4>{{0.0f, 0.0f, 0.0f, 0.0f}};
        };

        BlendState blendState{};

        std::vector<vk::DynamicState> dynamicStateVec;
    };


    class VKPipelineLayout {
    public:
        vk::PipelineLayout layout;
        VKWindow *pWin;

        explicit VKPipelineLayout(VKWindow *win, const std::vector<vk::DescriptorSetLayout> &descSetLayoutVec = {},
                                  const std::vector<vk::PushConstantRange> &pushConstVec = {});

        virtual ~VKPipelineLayout();

        VKPipelineLayout(const VKPipelineLayout &rhs) = delete;
        VKPipelineLayout &operator=(const VKPipelineLayout &rhs) = delete;

        VKPipelineLayout(VKPipelineLayout &&rhs) noexcept;
        VKPipelineLayout &operator=(VKPipelineLayout &&rhs) noexcept;
    };

    class VKPipelineCache {
    public:
        VKDevice *pDev;
        vk::PipelineCache cache;

        VKPipelineCache(VKDevice *gpu, const std::vector<uint8_t> &data = {});
        virtual ~VKPipelineCache();

        std::vector<uint8_t> getData();
        vk::Result mergeAndConsumeCaches(const std::vector<VKPipelineCache *> &caches); //!< Merges all these caches into this cache
    };

    class VKPipeline {
    public:

        VKDevice *pDev;
        vk::Pipeline pipeline;

        
        // NOTE: This constructor is specific *GRAPHICS* pipelines. Compute pipeline constructor is unimplemented.
        explicit VKPipeline(VKPipelineLayout *lyo, VKRenderPass *pass, uint32_t subpassIndex, const std::vector<VKShader *> &shaders, VKPipelineCache *cache = nullptr, const VKPipelineConfig& c = {});
        virtual ~VKPipeline();

        VKPipeline(const VKPipeline &rhs) = delete;
        VKPipeline &operator=(const VKPipeline &rhs) = delete;

        VKPipeline(VKPipeline &&rhs) noexcept;
        VKPipeline &operator=(VKPipeline &&rhs) noexcept;


    };

}

#endif // STMS_ENABLE_VULKAN

#endif //__STONEMASON_VK_PIPELINE_HPP
