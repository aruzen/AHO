#pragma once
#include <VSL/vulkan/swapchain.hpp>

namespace VSL_NAMESPACE::lifetime::detail {
    struct SwapchainAccess {
        static const OwnedRef<impl::SwapchainImpl>& Owned(const SwapchainAccessor& value) { return value._owner; }
        static OwnedRef<impl::SwapchainImpl>& Owned(SwapchainAccessor& value) { return value._owner; }
        static OwnedRef<impl::SwapchainImpl> Owned(SwapchainAccessor&& value) { return std::move(value._owner); }
        static OwnedRef<impl::SwapchainImpl> Owned(const SwapchainAccessor&& value) { return value._owner; }
        static SwapchainAccessor FromOwned(OwnedRef<impl::SwapchainImpl> owner, std::vector<ImageAccessor> images = {}) {
            return SwapchainAccessor(std::move(owner), std::move(images));
        }
        static SwapchainAccessor FromShared(std::shared_ptr<impl::SwapchainImpl> owner, std::vector<ImageAccessor> images = {});
    };
} // namespace VSL_NAMESPACE::lifetime::detail
