#pragma once
#include <VSL/define.hpp>

namespace VSL_NAMESPACE {
    struct ComputePhase;
}
namespace VSL_NAMESPACE::lifetime {
    // 同期callback後にPhase自身の寿命を検査する。Phaseを所有せず、stack上でだけ使う。
    class PhaseCall {
        ComputePhase* _phase;
        PhaseCall* _previous;
        bool _alive = true;

    public:
        explicit PhaseCall(ComputePhase& phase) noexcept;
        ~PhaseCall();
        PhaseCall(const PhaseCall&) = delete;
        PhaseCall& operator=(const PhaseCall&) = delete;
        bool alive() const noexcept { return _alive; }
        static void Invalidate(ComputePhase& phase) noexcept;
    };
} // namespace VSL_NAMESPACE::lifetime
