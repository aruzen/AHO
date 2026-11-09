#pragma once
#include <VSL/define.hpp>
#include <vulkan/vulkan.hpp>
#include <cstdint>

namespace VSL_NAMESPACE::operation {
    struct PresentationNativeCalls {
        void* context = nullptr;
        VkResult (*present)(void*, VkQueue, const VkPresentInfoKHR*) noexcept = nullptr;
        VkResult (*poll)(void*, VkDevice, VkFence) noexcept = nullptr;
        VkResult (*wait)(void*, VkDevice, VkFence, std::uint64_t) noexcept = nullptr;
        VkResult (*acquire)(void*, VkDevice, VkSwapchainKHR, std::uint64_t, VkSemaphore, VkFence, std::uint32_t*) noexcept = nullptr;
        VkResult (*release)(void*, VkDevice, const VkReleaseSwapchainImagesInfoEXT*) noexcept = nullptr;
    };
    // 故障注入は呼出しスレッドのscopeに限定し、Impl／Recordへ保存しない。
    class ScopedPresentationNativeCalls {
        PresentationNativeCalls _calls;
        const PresentationNativeCalls* _previous;

    public:
        explicit ScopedPresentationNativeCalls(PresentationNativeCalls calls) noexcept;
        ScopedPresentationNativeCalls(const ScopedPresentationNativeCalls&) = delete;
        ScopedPresentationNativeCalls& operator=(const ScopedPresentationNativeCalls&) = delete;
        ~ScopedPresentationNativeCalls();
    };
    vk::Result acquire_next_image(vk::Device device, vk::SwapchainKHR swapchain, std::uint64_t timeout,
                                  vk::Semaphore available, vk::Fence fence, std::uint32_t& image) noexcept;
    vk::Result queue_present(vk::Queue queue, vk::SwapchainKHR swapchain, std::uint32_t image,
                             vk::Semaphore finished, vk::Fence present_fence = {}) noexcept;
    vk::Result poll_present_fence(vk::Device device, vk::Fence fence) noexcept;
    vk::Result wait_present_fence(vk::Device device, vk::Fence fence, std::uint64_t timeout) noexcept;
    vk::Result release_swapchain_image(vk::Device device, vk::SwapchainKHR swapchain, std::uint32_t image) noexcept;
} // namespace VSL_NAMESPACE::operation
