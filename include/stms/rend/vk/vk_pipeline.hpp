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
#include <shaderc/shaderc.hpp>

#include "stms/rend/vk/vk_window.hpp"

namespace stms {
    /// Simple wrapper around `shaderc_optimization_level`, see shaderc docs.
    enum SPIRVOptimizationLevel {
        eNone = shaderc_optimization_level_zero, //!< Do not optimize SPIRV bytecode
        eSize = shaderc_optimization_level_size, //!< Reduce the size of the resulting code
        eSpeed = shaderc_optimization_level_performance //!< Optimize for performance at runtime
    };

    typedef vk::ShaderStageFlags VKShaderStage;
    typedef vk::ShaderStageFlagBits VKShaderStageBits;
    typedef shaderc_shader_kind SPIRVShaderStage; // TODO: Make a wrapper, because the naming scheme is different.

//    /**
//     * @brief Compile a GLSL source string into SPIR-V bytecode, ready for passing into a `VKShader`
//     * @param glslSrc Source code of the shader.
//     * @param stage Shader stage of the shader (i.e. is it a fragment shader? vertex shader? geometry shader? etc)
//     * @param name Name of the shader. See shaderc docs
//     * @param o Optimization level shader. See docs for `SPIRVOptimizationLevel`
//     * @throws If `stms::exceptionLevel > 0`, this will throw `std::runtime_error` if shader compilation failed. Otherwise
//     *         a blank string is returned.
//     * @return Compiled bytecode
//     */
//    std::string compileToSpirv(const std::string &glslSrc, SPIRVShaderStage stage, const std::string &name, SPIRVOptimizationLevel o);

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
         *                      (or `VkShaderStageBits`)
         * @param entryPoint Entry point of the shader, by default "`main`".
         */
        VKShader(VKDevice *dev, const std::string &bytecode, const VKShaderStageBits& shaderStage, const std::string &entryPoint = "main");
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
