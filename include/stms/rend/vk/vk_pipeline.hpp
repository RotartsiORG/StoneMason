//
// Created by grant on 8/9/20.
//

#pragma once

#ifndef __STONEMASON_VK_PIPELINE_HPP
#define __STONEMASON_VK_PIPELINE_HPP

#ifdef STMS_ENABLE_VULKAN

#include <vector>
#include <string>

namespace stms {
    typedef std::vector<char> SPIRV;

    class VkShader {
    public:
        VkShader(const SPIRV &bytecode);
    };
}

#endif // STMS_ENABLE_VULKAN

#endif //__STONEMASON_VK_PIPELINE_HPP
