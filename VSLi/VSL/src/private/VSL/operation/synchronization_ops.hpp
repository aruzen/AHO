#pragma once
#include <VSL/vulkan/_pimpls.hpp>
#include <VSL/operation/synchronization_create_info.hpp>
#include <span>

namespace VSL_NAMESPACE::operation {
    void initialize_semaphore(impl::SemaphoreImpl& semaphore, impl::LogicalDeviceImpl& device, SemaphoreCreateInfo info);
    void initialize_fence(impl::FenceImpl& fence, impl::LogicalDeviceImpl& device, FenceCreateInfo info);
    void validate_synchronization_count(vk::Device device, size_t count);
    std::vector<vk::Semaphore> create_semaphores(vk::Device device, size_t count);
    std::vector<vk::Fence> create_fences(vk::Device device, size_t count, bool signaled);
    void destroy_semaphores(vk::Device device, std::span<const vk::Semaphore> semaphores) noexcept;
    void destroy_fences(vk::Device device, std::span<const vk::Fence> fences) noexcept;
    vk::Result wait_fences(vk::Device device, std::span<const vk::Fence> fences, std::uint64_t timeout) noexcept;
    void reset_fences(vk::Device device, std::span<const vk::Fence> fences);
} // namespace VSL_NAMESPACE::operation
