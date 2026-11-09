#pragma once
#include <VSL/define.hpp>
#include <vulkan/vulkan.hpp>
#include <cstdint>
#include <span>

namespace VSL_NAMESPACE::operation {
    struct SubmissionNativeCalls {
        void* context = nullptr;
        VkResult (*wait_idle)(void*, VkDevice) noexcept = nullptr;
        VkResult (*wait_fences)(void*, VkDevice, std::uint32_t, const VkFence*, VkBool32, std::uint64_t) noexcept = nullptr;
        VkResult (*queue_submit)(void*, VkQueue, const VkSubmitInfo*, VkFence) noexcept = nullptr;
        VkResult (*fence_status)(void*, VkDevice, VkFence) noexcept = nullptr;
    };
    class ScopedSubmissionNativeCalls {
        SubmissionNativeCalls _calls;
        const SubmissionNativeCalls* _previous;

    public:
        explicit ScopedSubmissionNativeCalls(SubmissionNativeCalls calls) noexcept;
        ~ScopedSubmissionNativeCalls();
        ScopedSubmissionNativeCalls(const ScopedSubmissionNativeCalls&) = delete;
        ScopedSubmissionNativeCalls& operator=(const ScopedSubmissionNativeCalls&) = delete;
    };
    vk::Result poll_fence(vk::Device device, vk::Fence fence) noexcept;
    vk::Result wait_fence(vk::Device device, vk::Fence fence, std::uint64_t timeout) noexcept;
    vk::Result wait_fences(vk::Device device, std::span<const vk::Fence> fences, std::uint64_t timeout) noexcept;
    vk::Result wait_device_idle(vk::Device device) noexcept;
    vk::Result queue_submit(vk::Queue queue, const vk::SubmitInfo& info, vk::Fence fence) noexcept;
} // namespace VSL_NAMESPACE::operation
