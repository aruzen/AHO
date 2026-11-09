#pragma once

#include "../define.hpp"
#include "pv.hpp"

#include "device.hpp"
#include "synchronize.hpp"
#include "buffer_and_image_accessor.hpp"
#include "../operation/swapchain_create_info.hpp"
#include "../lifetime/resource_identity.hpp"
#include <optional>
#include <span>
#include <vector>

namespace VSL_NAMESPACE::lifetime::detail {
    struct SwapchainAccess;
}

namespace VSL_NAMESPACE {
    struct SwapchainAccessor {
    private:
        friend struct lifetime::detail::SwapchainAccess;
        lifetime::OwnedRef<impl::SwapchainImpl> _owner;
        std::vector<ImageAccessor> _images;
        SwapchainAccessor(lifetime::OwnedRef<impl::SwapchainImpl> owner, std::vector<ImageAccessor> images)
            : _owner(std::move(owner)), _images(std::move(images)) {}

    public:
        SwapchainAccessor() = default;
        SwapchainAccessor(const SwapchainAccessor&) = default;
        SwapchainAccessor(SwapchainAccessor&& source) noexcept { swap(source); }
        SwapchainAccessor& operator=(SwapchainAccessor replacement) noexcept {
            swap(replacement);
            return *this;
        }
        void swap(SwapchainAccessor& other) noexcept {
            _owner.swap(other._owner);
            _images.swap(other._images);
        }
        explicit operator bool() const noexcept { return bool(_owner); }
        SwapchainIdentity identity() const noexcept;
        ImageAccessor image(size_t index) const { return _images.at(index); }
        std::span<const ImageAccessor> images() const& { return _images; }
        std::span<const ImageAccessor> images() const&& = delete;

        size_t get_swap_image_size() const noexcept { return _images.size(); }
        data_format::Format get_format() const;
        lifetime::OwnerRef owner_ref() const;
        LogicalDeviceAccessor get_device() const;
        Surface get_surface() const;
        SwapchainState state() const;
        PresentFencePolicy present_fences() const;
        SemaphoreHolder finished_semaphores() const;
        void require_image_usage(ImageType usage) const;
    };

    struct Swapchain : public SwapchainAccessor {
        Swapchain(const Swapchain&) = default;
        Swapchain(Swapchain&&) noexcept = default;
        Swapchain& operator=(Swapchain replacement) noexcept {
            swap(replacement);
            return *this;
        }
        Swapchain(LogicalDeviceAccessor device, std::shared_ptr<Surface> surface,
                  std::optional<VSL_NAMESPACE::SwapchainAccessor> old = std::nullopt,
                  std::optional<int> width = std::nullopt, std::optional<int> height = std::nullopt,
                  ImageType usage = ImageType::COLOR_ATTACHMENT);
    };
} // namespace VSL_NAMESPACE
