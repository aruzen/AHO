#pragma once
#include <VSL/operation/swapchain_ops.hpp>
#include <VSL/operation/image_ops.hpp>
#include <VSL/lifetime/owner_ref.hpp>
#include <VSL/vulkan/swapchain.hpp>
#include <VSL/storage/arena_memory.hpp>
#include <functional>

namespace VSL_NAMESPACE::storage {
    class SwapchainLayout;
}
namespace VSL_NAMESPACE::lifetime {
    struct SwapchainSlotStorage {
        alignas(impl::SwapchainImpl) std::byte value[sizeof(impl::SwapchainImpl)];
    };
    using SwapchainRegionFactory = std::function<std::shared_ptr<storage::detail::CPURegion>(const storage::SwapchainLayout&)>;
    void construct_swapchain(void* storage, operation::PendingSwapchain& pending) noexcept;
    impl::SwapchainImpl& swapchain_at(void* storage);
    void destroy_swapchain(void* storage) noexcept;
    void require_swapchain_owner(const SwapchainAccessor& source);
    void require_swapchain_owner(const std::shared_ptr<impl::SwapchainImpl>& source);
    std::shared_ptr<impl::ImageImpl> swapchain_image(const std::shared_ptr<impl::SwapchainImpl>& source, size_t index);
    std::shared_ptr<impl::SemaphoreImpl> swapchain_finished(const std::shared_ptr<impl::SwapchainImpl>& source);
    SwapchainAccessor make_swapchain(LogicalDeviceAccessor device, Surface surface, SwapchainCreateInfo info,
                                     const std::optional<SwapchainAccessor>& old, const SwapchainRegionFactory& memory,
                                     const operation::SwapchainNativeCalls& calls = {}, const operation::ImageViewNativeCalls& image_calls = {},
                                     SwapchainCreateProgress* progress = nullptr);
} // namespace VSL_NAMESPACE::lifetime
