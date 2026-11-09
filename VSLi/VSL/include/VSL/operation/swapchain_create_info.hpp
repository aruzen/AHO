#pragma once
#include "image_create_info.hpp"
#include "device_create_info.hpp"
#include <optional>

namespace VSL_NAMESPACE {
    enum class SwapchainState {
        UNINITIALIZED,
        ACTIVE,
        RETIRED,
        UNUSABLE,
        RECOVERY_REQUIRED
    };
    struct SwapchainCreateInfo {
        // 省略した成分はmanaged OS windowのframebufferから取得する。
        std::optional<std::uint32_t> width, height;
        ImageType usage = ImageType::COLOR_ATTACHMENT;
        PresentFencePolicy present_fences = PresentFencePolicy::LEGACY;
    };
} // namespace VSL_NAMESPACE
