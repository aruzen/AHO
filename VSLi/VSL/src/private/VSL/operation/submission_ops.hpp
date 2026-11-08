#pragma once
#include <VSL/define.hpp>
#include <vulkan/vulkan.hpp>
#include <cstdint>

namespace VSL_NAMESPACE::operation {
    vk::Result poll_fence(vk::Device device, vk::Fence fence) noexcept;
    vk::Result wait_fence(vk::Device device, vk::Fence fence, std::uint64_t timeout) noexcept;
    vk::Result wait_device_idle(vk::Device device) noexcept;
} // namespace VSL_NAMESPACE::operation
