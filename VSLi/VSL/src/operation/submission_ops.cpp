#include <VSL/operation/submission_ops.hpp>

namespace VSL_NAMESPACE::operation {
    namespace {
        thread_local const SubmissionNativeCalls* NATIVE_CALLS = nullptr;
    }
    ScopedSubmissionNativeCalls::ScopedSubmissionNativeCalls(SubmissionNativeCalls calls) noexcept
        : _calls(calls), _previous(NATIVE_CALLS) {
        NATIVE_CALLS = &_calls;
    }
    ScopedSubmissionNativeCalls::~ScopedSubmissionNativeCalls() {
        NATIVE_CALLS = _previous;
    }
    vk::Result poll_fence(vk::Device device, vk::Fence fence) noexcept {
        return static_cast<vk::Result>(VULKAN_HPP_DEFAULT_DISPATCHER.vkGetFenceStatus(static_cast<VkDevice>(device), static_cast<VkFence>(fence)));
    }
    vk::Result wait_fence(vk::Device device, vk::Fence fence, std::uint64_t timeout) noexcept {
        return wait_fences(device, {&fence, 1}, timeout);
    }
    vk::Result wait_fences(vk::Device device, std::span<const vk::Fence> fences, std::uint64_t timeout) noexcept {
        const auto count = static_cast<std::uint32_t>(fences.size());
        const auto* handles = reinterpret_cast<const VkFence*>(fences.data());
        return static_cast<vk::Result>(NATIVE_CALLS && NATIVE_CALLS->wait_fences
                                           ? NATIVE_CALLS->wait_fences(NATIVE_CALLS->context, device, count, handles, VK_TRUE, timeout)
                                           : VULKAN_HPP_DEFAULT_DISPATCHER.vkWaitForFences(device, count, handles, VK_TRUE, timeout));
    }
    vk::Result wait_device_idle(vk::Device device) noexcept {
        if (NATIVE_CALLS && NATIVE_CALLS->wait_idle)
            return static_cast<vk::Result>(NATIVE_CALLS->wait_idle(NATIVE_CALLS->context, device));
        return static_cast<vk::Result>(VULKAN_HPP_DEFAULT_DISPATCHER.vkDeviceWaitIdle(static_cast<VkDevice>(device)));
    }
} // namespace VSL_NAMESPACE::operation
