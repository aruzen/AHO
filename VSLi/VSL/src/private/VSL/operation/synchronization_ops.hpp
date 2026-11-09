#pragma once
#include <VSL/vulkan/_pimpls.hpp>
#include <VSL/operation/synchronization_create_info.hpp>
#include <VSL/operation/submission_ops.hpp>
#include <span>

namespace VSL_NAMESPACE::operation {
    struct SynchronizationNativeCalls {
        void* context = nullptr;
        VkResult (*create_semaphore)(void*, VkDevice, const VkSemaphoreCreateInfo*, VkSemaphore*) noexcept = nullptr;
        void (*destroy_semaphore)(void*, VkDevice, VkSemaphore) noexcept = nullptr;
        VkResult (*reset_fences)(void*, VkDevice, std::uint32_t, const VkFence*) noexcept = nullptr;
    };
    class ScopedSynchronizationNativeCalls {
        SynchronizationNativeCalls _calls;
        const SynchronizationNativeCalls* _previous;

    public:
        explicit ScopedSynchronizationNativeCalls(SynchronizationNativeCalls calls) noexcept;
        ~ScopedSynchronizationNativeCalls();
        ScopedSynchronizationNativeCalls(const ScopedSynchronizationNativeCalls&) = delete;
        ScopedSynchronizationNativeCalls& operator=(const ScopedSynchronizationNativeCalls&) = delete;
    };
    void initialize_semaphore(impl::SemaphoreImpl& semaphore, impl::LogicalDeviceImpl& device, SemaphoreCreateInfo info);
    void initialize_fence(impl::FenceImpl& fence, impl::LogicalDeviceImpl& device, FenceCreateInfo info);
    void validate_synchronization_count(vk::Device device, size_t count);
    std::vector<vk::Semaphore> create_semaphores(vk::Device device, size_t count);
    std::vector<vk::Fence> create_fences(vk::Device device, size_t count, bool signaled);
    void destroy_semaphores(vk::Device device, std::span<const vk::Semaphore> semaphores) noexcept;
    void destroy_fences(vk::Device device, std::span<const vk::Fence> fences) noexcept;
    void reset_fences(vk::Device device, std::span<const vk::Fence> fences);
} // namespace VSL_NAMESPACE::operation
