#include <VSL/vulkan/swapchain.hpp>
#include <VSL/lifetime/swapchain_arena.hpp>
#include <VSL/lifetime/swapchain_storage.hpp>
#include <VSL/lifetime/device_owner.hpp>
#include <VSL/lifetime/owner_access.hpp>
#include <stdexcept>

namespace VSL_NAMESPACE {
    void SwapchainAccessor::require_image_usage(ImageType usage) const {
        const auto source = data_;
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
        auto result = std::move(arena).view();
        data_ = std::move(result.data_);
        images = std::move(result.images);
    }
    size_t SwapchainAccessor::get_swap_image_size() {
        return images.size();
    }
    data_format::Format SwapchainAccessor::get_format() const {
        if (!data_)
            throw std::invalid_argument("Swapchain format requires a description");
        const auto type = static_cast<data_format::FormatTypes>(data_->swap_chain_image_format);
        const auto format = data_format::convert(type);
        return format.type() == data_format::FormatTypes::UNDEFINED ? data_format::Format(type, 0) : format;
    }
    lifetime::OwnerRef SwapchainAccessor::owner_ref() const {
        return lifetime::detail::owned_ref(data_, data_ ? data_->owner_node : nullptr);
    }
    LogicalDeviceAccessor SwapchainAccessor::get_device() const {
        // offscreenの旧descriptionも通常の子getterと同じopaque契約を維持する。
        return lifetime::device_from_child(data_);
    }
    Surface SwapchainAccessor::get_surface() const {
        lifetime::require_swapchain_owner(*this);
        auto* parent = data_->surface;
        if (!parent || !parent->surface || !parent->owner_node || parent->owner_node->retiring())
            throw std::logic_error("Swapchain has no Surface owner");
        return Surface(parent->owner_node->owner_ref().alias(parent).share());
    }
    SwapchainState SwapchainAccessor::state() const {
        return data_ ? data_->state : SwapchainState::UNINITIALIZED;
    }
    PresentFencePolicy SwapchainAccessor::present_fences() const {
        if (!data_)
            throw std::invalid_argument("Swapchain has no presentation policy");
        return data_->present_fences;
    }
    SemaphoreHolder SwapchainAccessor::finished_semaphores() const {
        return {lifetime::swapchain_finished(data_)};
    }
} // namespace VSL_NAMESPACE
