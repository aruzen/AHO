#include <VSL/vulkan/swapchain.hpp>
#include <VSL/lifetime/swapchain_access.hpp>
#include <VSL/lifetime/owner_validation.hpp>
#include <VSL/lifetime/identity_access.hpp>
#include <VSL/lifetime/synchronization_access.hpp>
#include <VSL/lifetime/swapchain_arena.hpp>
#include <VSL/lifetime/swapchain_storage.hpp>
#include <VSL/lifetime/device_owner.hpp>
#include <VSL/lifetime/owner_access.hpp>
#include <stdexcept>

namespace VSL_NAMESPACE {
    SwapchainAccessor lifetime::detail::SwapchainAccess::FromShared(std::shared_ptr<impl::SwapchainImpl> owner, std::vector<ImageAccessor> images) {
        const auto* node = owner ? owner->owner_node : nullptr;
        return FromOwned(owned_ref(std::move(owner), node), std::move(images));
    }
    SwapchainIdentity SwapchainAccessor::identity() const noexcept {
        return lifetime::detail::IdentityAccess::From<SwapchainIdentity>(_owner.get());
    }
    void SwapchainAccessor::require_image_usage(ImageType usage) const {
        const auto source = _owner;
        lifetime::require_swapchain_owner(source);
        operation::require_swapchain_image_usage(*source, usage);
    }
    Swapchain::Swapchain(LogicalDeviceAccessor device, std::shared_ptr<Surface> surface, std::optional<SwapchainAccessor> old,
                         std::optional<int> width, std::optional<int> height, ImageType usage) {
        if (!surface)
            throw std::invalid_argument("Swapchain requires a Surface owner");
        SwapchainCreateInfo info;
        info.usage = usage;
        if (width) {
            if (*width <= 0)
                throw std::invalid_argument("Swapchain width must be positive");
            info.width = static_cast<std::uint32_t>(*width);
        }
        if (height) {
            if (*height <= 0)
                throw std::invalid_argument("Swapchain height must be positive");
            info.height = static_cast<std::uint32_t>(*height);
        }
        lifetime::SwapchainArena arena(std::move(device), *surface, info, std::move(old));
        SwapchainAccessor::operator=(std::move(arena).view());
    }
    data_format::Format SwapchainAccessor::get_format() const {
        if (!_owner)
            throw std::invalid_argument("Swapchain format requires a description");
        const auto type = static_cast<data_format::FormatTypes>(_owner->swap_chain_image_format);
        const auto format = data_format::convert(type);
        return format.type() == data_format::FormatTypes::UNDEFINED ? data_format::Format(type, 0) : format;
    }
    lifetime::OwnerRef SwapchainAccessor::owner_ref() const {
        lifetime::detail::require_live_owner(_owner);
        return _owner;
    }
    LogicalDeviceAccessor SwapchainAccessor::get_device() const {
        // offscreenの旧descriptionも通常の子getterと同じopaque契約を維持する。
        return lifetime::device_from_child(_owner);
    }
    Surface SwapchainAccessor::get_surface() const {
        lifetime::require_swapchain_owner(*this);
        auto* parent = _owner->surface;
        if (!parent || !parent->surface || !parent->owner_node || parent->owner_node->retiring())
            throw std::logic_error("Swapchain has no Surface owner");
        return Surface(parent->owner_node->owner_ref().alias(parent).share());
    }
    SwapchainState SwapchainAccessor::state() const {
        return _owner ? _owner->state : SwapchainState::UNINITIALIZED;
    }
    PresentFencePolicy SwapchainAccessor::present_fences() const {
        if (!_owner)
            throw std::invalid_argument("Swapchain has no presentation policy");
        return _owner->present_fences;
    }
    SemaphoreHolder SwapchainAccessor::finished_semaphores() const {
        lifetime::require_swapchain_owner(_owner);
        return lifetime::detail::SynchronizationAccess::FromShared(lifetime::swapchain_finished(lifetime::detail::OwnerAccess::Shared(_owner)));
    }
} // namespace VSL_NAMESPACE
