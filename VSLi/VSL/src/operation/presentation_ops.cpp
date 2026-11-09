#include <VSL/operation/presentation_ops.hpp>
#include <VSL/operation/submission_ops.hpp>

namespace VSL_NAMESPACE::operation {
    namespace {
        thread_local const PresentationNativeCalls* NATIVE_CALLS = nullptr;
    }
    ScopedPresentationNativeCalls::ScopedPresentationNativeCalls(PresentationNativeCalls calls) noexcept
        : _calls(calls), _previous(NATIVE_CALLS) {
        NATIVE_CALLS = &_calls;
    }
    ScopedPresentationNativeCalls::~ScopedPresentationNativeCalls() {
        NATIVE_CALLS = _previous;
    }
    vk::Result acquire_next_image(vk::Device device, vk::SwapchainKHR swapchain, std::uint64_t timeout,
                                  vk::Semaphore available, vk::Fence fence, std::uint32_t& image) noexcept {
        return static_cast<vk::Result>(NATIVE_CALLS && NATIVE_CALLS->acquire
                                           ? NATIVE_CALLS->acquire(NATIVE_CALLS->context, device, swapchain, timeout, available, fence, &image)
                                           : VULKAN_HPP_DEFAULT_DISPATCHER.vkAcquireNextImageKHR(device, swapchain, timeout, available, fence, &image));
    }
    vk::Result queue_present(vk::Queue queue, vk::SwapchainKHR swapchain, std::uint32_t image,
                             vk::Semaphore finished, vk::Fence present_fence) noexcept {
        const VkSwapchainKHR native_chain = swapchain;
        const VkSemaphore native_finished = finished;
        const VkFence native_fence = present_fence;
        const VkSwapchainPresentFenceInfoEXT fence_info{
            .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_PRESENT_FENCE_INFO_EXT,
            .pNext = nullptr,
            .swapchainCount = 1,
            .pFences = &native_fence,
        };
        const VkPresentInfoKHR info{
            .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .pNext = present_fence ? &fence_info : nullptr,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &native_finished,
            .swapchainCount = 1,
            .pSwapchains = &native_chain,
            .pImageIndices = &image,
            .pResults = nullptr,
        };
        return static_cast<vk::Result>(NATIVE_CALLS && NATIVE_CALLS->present ? NATIVE_CALLS->present(NATIVE_CALLS->context, queue, &info)
                                                                             : VULKAN_HPP_DEFAULT_DISPATCHER.vkQueuePresentKHR(queue, &info));
    }
    vk::Result poll_present_fence(vk::Device device, vk::Fence fence) noexcept {
        return NATIVE_CALLS && NATIVE_CALLS->poll ? static_cast<vk::Result>(NATIVE_CALLS->poll(NATIVE_CALLS->context, device, fence)) : poll_fence(device, fence);
    }
    vk::Result wait_present_fence(vk::Device device, vk::Fence fence, std::uint64_t timeout) noexcept {
        return NATIVE_CALLS && NATIVE_CALLS->wait ? static_cast<vk::Result>(NATIVE_CALLS->wait(NATIVE_CALLS->context, device, fence, timeout)) : wait_fence(device, fence, timeout);
    }
} // namespace VSL_NAMESPACE::operation
