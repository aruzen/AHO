#pragma once

#include "../define.hpp"
#include "pv.hpp"

#include "device.hpp"
#include <optional>

namespace VSL_NAMESPACE {
    struct SwapchainAccessor {
        std::shared_ptr<_impl::Swapchain_impl> _data;

        size_t getSwapImageSize();
    };

    struct Swapchain : public SwapchainAccessor {
        Swapchain(LogicalDeviceAccessor device, std::shared_ptr<Surface> surface,
                  std::optional<int> width = std::nullopt, std::optional<int> height = std::nullopt);

        Swapchain(LogicalDeviceAccessor device, std::shared_ptr<Surface> surface, SwapchainAccessor old,
                  std::optional<int> width = std::nullopt, std::optional<int> height = std::nullopt);
    };
}