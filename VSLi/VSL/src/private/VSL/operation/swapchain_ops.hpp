#pragma once
#include <VSL/vulkan/_pimpls.hpp>
#include <VSL/operation/swapchain_create_info.hpp>

namespace VSL_NAMESPACE::operation {
    struct SwapchainNativeCalls {
        void* context = nullptr;
        VkResult (*create)(void*, VkDevice, const VkSwapchainCreateInfoKHR*, VkSwapchainKHR*) = nullptr;
        VkResult (*images)(void*, VkDevice, VkSwapchainKHR, std::uint32_t*, VkImage*) = nullptr;
        void (*destroy)(void*, VkDevice, VkSwapchainKHR) noexcept = nullptr;
    };
    // 呼出し中だけのnative所有。親の寿命は外側lifetimeが保証する。
    struct PendingSwapchain {
        impl::LogicalDeviceImpl* device = nullptr;
        impl::SurfaceImpl* surface = nullptr;
        vk::Device native_device;
        vk::SwapchainKHR chain;
        vk::SurfaceFormatKHR format{};
        vk::PresentModeKHR mode{};
        vk::Extent2D extent{};
        vk::SurfaceTransformFlagBitsKHR transform{};
        ImageType usage = ImageType::NONE;
        PresentFencePolicy present_fences = PresentFencePolicy::LEGACY;
        std::vector<vk::Image> images;
        SwapchainNativeCalls calls;
        PendingSwapchain() = default;
        PendingSwapchain(const PendingSwapchain&) = delete;
        PendingSwapchain& operator=(const PendingSwapchain&) = delete;
        PendingSwapchain(PendingSwapchain&& other) noexcept;
        ~PendingSwapchain();
    };
    PendingSwapchain prepare_swapchain(impl::LogicalDeviceImpl& device, impl::SurfaceImpl& surface, const SwapchainCreateInfo& info,
                                       impl::SwapchainImpl* old = nullptr, const SwapchainNativeCalls& calls = {});
    void adopt_swapchain(impl::SwapchainImpl& target, PendingSwapchain& pending) noexcept;
    void destroy_swapchain(impl::SwapchainImpl& target) noexcept;
    void require_active_swapchain(const impl::SwapchainImpl& target);
    void require_replaceable_swapchain(const impl::SwapchainImpl& target);
} // namespace VSL_NAMESPACE::operation
