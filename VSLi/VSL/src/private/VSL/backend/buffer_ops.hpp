#pragma once
#include <VSL/vulkan/_pimpls.hpp>
#include <VSL/vulkan/buffer_create_info.hpp>
#include <VSL/contracts/buffer_access.hpp>

namespace VSL_NAMESPACE::backend {
    // CPU領域・所有者・submissionを選ばないnative操作。
    void initialize_buffer(impl::BufferImpl& target, impl::LogicalDeviceImpl& device, const BufferCreateInfo& info);
    void destroy_buffer(impl::BufferImpl& target) noexcept;
    void* map_buffer(impl::BufferImpl& target, size_t offset, size_t bytes);
    void synchronize_buffer(impl::BufferImpl& target, size_t offset, size_t bytes, bool flush);
    void validate_copy(const impl::BufferImpl& source, const impl::BufferImpl& destination,
                       size_t bytes, size_t source_offset, size_t destination_offset, bool host_read);
    void record_copy(vk::CommandBuffer command, const impl::BufferImpl& source, const impl::BufferImpl& destination,
                     size_t bytes, size_t source_offset, size_t destination_offset, const BufferCopyVisibility& visibility);
} // namespace VSL_NAMESPACE::backend
