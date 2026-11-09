#include <VSL/lifetime/facade_call.hpp>
#include <utility>

namespace VSL_NAMESPACE::lifetime {
    thread_local FacadeCall* FacadeCall::CURRENT = nullptr;
    FacadeCall::FacadeCall(const void* subject) noexcept
        : _subject(subject), _previous(std::exchange(CURRENT, this)) {}
    FacadeCall::~FacadeCall() {
        CURRENT = _previous;
    }
    void FacadeCall::Invalidate(const void* subject) noexcept {
        for (auto* call = CURRENT; call; call = call->_previous)
            if (call->_subject == subject)
                call->_alive = false;
    }
} // namespace VSL_NAMESPACE::lifetime
