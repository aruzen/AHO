#pragma once
#include <VSL/define.hpp>
#include <vulkan/vulkan.hpp>
#include <cstddef>

namespace VSL_NAMESPACE::impl {
    struct BufferImpl;
    struct ImageImpl;
} // namespace VSL_NAMESPACE::impl

namespace VSL_NAMESPACE::operation {
    void write_descriptor(vk::Device device, const vk::WriteDescriptorSet& write);
    void validate_descriptor_buffer(const impl::BufferImpl& buffer, vk::DescriptorType type, size_t offset, size_t bytes);
    void validate_descriptor_image(const impl::ImageImpl& image, vk::DescriptorType type);
} // namespace VSL_NAMESPACE::operation
