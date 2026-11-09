#pragma once
#include "../define.hpp"

namespace VSL_NAMESPACE {
    struct LogicalDeviceAccessor;
    struct Surface;
} // namespace VSL_NAMESPACE
namespace VSL_NAMESPACE::lifetime {
    enum class SurfaceRecoveryMode {
        POLL,
        WAIT
    };
    enum class SurfaceRecoveryStatus {
        COMPLETE,
        PENDING,
        DEVICE_LOST
    };
    SurfaceRecoveryStatus recover_surface(LogicalDeviceAccessor device, const Surface& surface,
                                          SurfaceRecoveryMode mode = SurfaceRecoveryMode::WAIT);
} // namespace VSL_NAMESPACE::lifetime
