#pragma once
#include <VSL/define.hpp>
#include <vulkan/vulkan.hpp>
#include <cstddef>
#include <span>

namespace VSL_NAMESPACE {
    struct DescriptorLayoutCreateInfo;
    struct DescriptorPoolCreateInfo;
} // namespace VSL_NAMESPACE

namespace VSL_NAMESPACE::impl {
    struct BufferImpl;
    struct ImageImpl;
    struct LogicalDeviceImpl;
    struct ResourceBindingLayoutImpl;
    struct GraphicResourcePoolImpl;
    struct GraphicResourceImpl;
} // namespace VSL_NAMESPACE::impl

namespace VSL_NAMESPACE::operation {
    void initialize_descriptor_layout(impl::ResourceBindingLayoutImpl& layout, impl::LogicalDeviceImpl& device, const DescriptorLayoutCreateInfo& info);
    void destroy_descriptor_layout(impl::ResourceBindingLayoutImpl& layout) noexcept;
    void initialize_descriptor_pool(impl::GraphicResourcePoolImpl& pool, impl::LogicalDeviceImpl& device, const DescriptorPoolCreateInfo& info);
    void destroy_descriptor_pool(impl::GraphicResourcePoolImpl& pool) noexcept;
    vk::Result allocate_descriptor_sets(impl::GraphicResourcePoolImpl& pool, std::span<impl::GraphicResourceImpl* const> sets);
    void destroy_descriptor_set(impl::GraphicResourceImpl& resource) noexcept;
    void write_descriptor(vk::Device device, const vk::WriteDescriptorSet& write);
    void validate_descriptor_buffer(const impl::BufferImpl& buffer, vk::DescriptorType type, size_t offset, size_t bytes);
    void validate_descriptor_image(const impl::ImageImpl& image, vk::DescriptorType type);
} // namespace VSL_NAMESPACE::operation
