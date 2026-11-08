#pragma once
#include "../storage/swapchain_layout.hpp"
#include "../storage/arena_memory.hpp"
#include "../vulkan/swapchain.hpp"
#include <functional>

namespace VSL_NAMESPACE::lifetime {
    // native生成後の実数Mに対応する。呼出し中だけ使い、ownerへ保存しない。
    using SwapchainMemoryFactory = std::function<storage::ArenaMemory(const storage::SwapchainLayout&)>;
    class SwapchainArena {
        struct ValidatedStorage {};
        SwapchainAccessor _view;
        SwapchainArena(LogicalDeviceAccessor device, Surface surface, SwapchainCreateInfo info,
                       std::optional<SwapchainAccessor> old, SwapchainMemoryFactory memory, ValidatedStorage);

    public:
        SwapchainArena(LogicalDeviceAccessor device, Surface surface, SwapchainCreateInfo info = {},
                       std::optional<SwapchainAccessor> old = {}, SwapchainMemoryFactory memory = {});
        SwapchainAccessor view() const& { return _view; }
        SwapchainAccessor view() && { return std::move(_view); }
        OwnerRef owner_ref() const;
        size_t image_count() const;
    };

    inline SwapchainArena::SwapchainArena(LogicalDeviceAccessor device, Surface surface, SwapchainCreateInfo info,
                                          std::optional<SwapchainAccessor> old, SwapchainMemoryFactory memory)
        : SwapchainArena(std::move(device), std::move(surface), std::move(info), std::move(old), std::move(memory),
                         (storage::detail::require_resource_layout(std::integral_constant<std::uint64_t, storage::RESOURCE_LAYOUT_ABI>{}), ValidatedStorage{})) {}
} // namespace VSL_NAMESPACE::lifetime
