#pragma once
#include <VSL/sync/resource_use_state.hpp>

namespace VSL_NAMESPACE::sync {
    // 呼出し側が実体を保持する内部記録用。完了観測とcallbackは別の境界へ延期する。
    struct ResourceUseAccess {
        static void TrackPending(const ResourceUseState& state, const Completion& completion) { state.track_pending(completion); }
        static bool RequiresKnown(const ResourceUseState& state) noexcept { return state._uncertain == UncertainUsePolicy::REQUIRE_RECREATION; }
        static void RequireKnown(const ResourceUseState& state) { state.require_known_use(); }
    };
} // namespace VSL_NAMESPACE::sync
