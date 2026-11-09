#pragma once
#include <VSL/sync/synchronization_state.hpp>

namespace VSL_NAMESPACE::sync {
    enum class AcquisitionStatus {
        UNARMED,
        ACQUIRED,
        DELEGATED,
        RETURNED,
        ABANDONED,
        DEVICE_LOST,
        NOT_ACQUIRED,
        UNCERTAIN
    };
    struct AcquisitionState {
        AcquisitionStatus status = AcquisitionStatus::UNARMED;
        SubmissionNativeState drain;
        bool acquired = false, native_call_active = false, abandon_requested = false;
        bool release_attempted = false;
    };
} // namespace VSL_NAMESPACE::sync
