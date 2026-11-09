#pragma once

#include "../define.hpp"
#include "pv.hpp"

#include "device.hpp"
#include "synchronize.hpp"
#include "buffer_and_image_accessor.hpp"
#include "../operation/swapchain_create_info.hpp"
#include <optional>

namespace VSL_NAMESPACE {
    struct SwapchainAccessor {
        std::shared_ptr<impl::SwapchainImpl> data_;
        std::vector<ImageAccessor> images;

        size_t get_swap_image_size();
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
        Swapchain(LogicalDeviceAccessor device, std::shared_ptr<Surface> surface,
                  std::optional<VSL_NAMESPACE::SwapchainAccessor> old = std::nullopt,
                  std::optional<int> width = std::nullopt, std::optional<int> height = std::nullopt,
                  ImageType usage = ImageType::COLOR_ATTACHMENT);
    };
} // namespace VSL_NAMESPACE
