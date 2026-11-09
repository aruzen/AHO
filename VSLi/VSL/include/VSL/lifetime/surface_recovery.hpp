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
    // 登録はNative呼出し前に行う。弱い元親だけを保存し、以後の使用開始は呼出し側で止める。
    void queue_surface_retirement(LogicalDeviceAccessor device, const Surface& surface);
    SurfaceRecoveryStatus collect_surface_retirements(LogicalDeviceAccessor device,
                                                      SurfaceRecoveryMode mode = SurfaceRecoveryMode::POLL);
} // namespace VSL_NAMESPACE::lifetime
