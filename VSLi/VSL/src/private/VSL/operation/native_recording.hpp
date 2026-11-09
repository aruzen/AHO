#pragma once
#include <VSL/vulkan/pv.hpp>
#include <vulkan/vulkan_core.h>
#include <cstdint>

namespace VSL_NAMESPACE::operation {
    VkCommandBuffer native_command_buffer(const impl::CommandBufferImpl& buffer, std::uint32_t slot);
} // namespace VSL_NAMESPACE::operation
