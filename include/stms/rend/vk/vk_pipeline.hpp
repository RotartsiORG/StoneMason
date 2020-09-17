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
    private:
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
         * @param bytecode SPIR-V bytecode of the shader, either loaded from a file or compiled using `compileToSpirv`
         * @param shaderStage The stage of the shader. See Vulkan documentation for `VkShaderStageFlagBits`
         * @param entryPoint Entry point of the shader, by default "`main`".
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
}

#endif // STMS_ENABLE_VULKAN

#endif //__STONEMASON_VK_PIPELINE_HPP
