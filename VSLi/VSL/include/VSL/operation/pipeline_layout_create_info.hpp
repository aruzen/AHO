#pragma once
#include "../vulkan/shader.hpp"

namespace VSL_NAMESPACE {
    struct PushConstantRange {
        ShaderFlag target_shader;
        std::uint32_t offset = UINT32_MAX, size = UINT32_MAX;
    };
} // namespace VSL_NAMESPACE
