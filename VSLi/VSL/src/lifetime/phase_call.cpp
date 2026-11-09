#include <VSL/lifetime/phase_call.hpp>
#include <VSL/vulkan/phase.hpp>
#include <utility>
#include <stdexcept>

namespace VSL_NAMESPACE::lifetime {
    NativeCall::NativeCall(bool& active) : _active(active) {
        if (active)
            throw std::logic_error("Native call is already active");
        active = true;
    }
    NativeCall::~NativeCall() {
        _active = false;
    }
    PhaseCall::PhaseCall(ComputePhase& phase) noexcept
        : _phase(&phase), _previous(std::exchange(phase._active_call, this)) {}
    PhaseCall::~PhaseCall() {
        if (_alive)
            _phase->_active_call = _previous;
    }
    void PhaseCall::Invalidate(ComputePhase& phase) noexcept {
        for (auto* call = std::exchange(phase._active_call, nullptr); call; call = call->_previous)
            call->_alive = false;
    }
} // namespace VSL_NAMESPACE::lifetime
