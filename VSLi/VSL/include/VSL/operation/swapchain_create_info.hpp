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
    // 呼出し中だけ借用する。Native開始後の失敗でもtrueを保持し、再入／並行呼出しでは共有しない。
    struct SwapchainCreateProgress {
        bool native_started = false;
    };
} // namespace VSL_NAMESPACE
