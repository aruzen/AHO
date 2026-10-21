#pragma once

#include "../define.hpp"
#include "pv.hpp"

#include "device.hpp"
#include "buffer_and_image_accessor.hpp"
#include <optional>

namespace VSL_NAMESPACE {
    struct SwapchainAccessor {
        std::shared_ptr<impl::SwapchainImpl> data_;
        std::vector<ImageAccessor> images;

        size_t get_swap_image_size();
        data_format::Format get_format() const;
    };

    struct Swapchain : public SwapchainAccessor {
        Swapchain(LogicalDeviceAccessor device, std::shared_ptr<Surface> surface,
                  std::optional<VSL_NAMESPACE::SwapchainAccessor> old = std::nullopt,
                  std::optional<int> width = std::nullopt, std::optional<int> height = std::nullopt,
                  ImageType usage = ImageType::COLOR_ATTACHMENT);
    };
} // namespace VSL_NAMESPACE
