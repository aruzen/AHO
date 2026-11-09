#include <VSL/sync/device_call.hpp>
#include <utility>

namespace VSL_NAMESPACE::sync {
    thread_local const NativeDeviceCall* NativeDeviceCall::CURRENT = nullptr;
    NativeDeviceCall::NativeDeviceCall(VkDevice device) noexcept
        : _device(device), _previous(std::exchange(CURRENT, this)) {}
    NativeDeviceCall::~NativeDeviceCall() {
        finish();
    }
    void NativeDeviceCall::finish() noexcept {
        if (_active && CURRENT == this) {
            CURRENT = _previous;
            _active = false;
        }
    }
    bool NativeDeviceCall::Active(VkDevice device) noexcept {
        for (auto* current = CURRENT; current; current = current->_previous)
            if (current->_device == device)
                return true;
        return false;
    }
} // namespace VSL_NAMESPACE::sync
