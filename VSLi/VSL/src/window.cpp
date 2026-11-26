#include <VSL/window.hpp>
#include <VSL/window_plugin.hpp>
#include <VSL/lifetime/window_access.hpp>
#include <VSL/lifetime/window_runtime.hpp>
#include <VSL/operation/window_ops.hpp>
#include <chrono>
#include <stdexcept>

namespace VSL_NAMESPACE {
    PureWindow::PureWindow(std::string name, int width, int height) {
        lifetime::GLFWCallScope scope;
        auto state = std::make_shared<WindowData>();
        state->name = std::move(name);
        state->width = width;
        state->height = height;
        auto owner = lifetime::OSWindowOwner::Create(state->name, width, height);
        lifetime::detail::WindowAccess::Registry().push_back(state);
        state->window_handle = owner.native_handle();
        state->os_window = std::move(owner);
        _state = std::move(state);
    }

    PureWindow::PureWindow(std::shared_ptr<WindowData> state) noexcept : _state(std::move(state)) {}
    PureWindow::PureWindow(const PureWindow& other) noexcept : _state(other._state) {}

    PureWindow::PureWindow(PureWindow&& other) noexcept : _state(std::move(other._state)) {
        other.invalidate_calls();
    }

    PureWindow& PureWindow::operator=(PureWindow other) noexcept {
        swap(other);
        return *this;
    }

    PureWindow::~PureWindow() {
        invalidate_calls();
    }

    void PureWindow::swap(PureWindow& other) noexcept {
        if (*this == other)
            return;
        invalidate_calls();
        other.invalidate_calls();
        _state.swap(other._state);
    }

    void PureWindow::invalidate_calls() noexcept {
        while (_calls) {
            auto* call = _calls;
            _calls = call->_previous;
            call->_target = nullptr;
        }
    }

    PureWindow::Call::Call(const PureWindow& target) noexcept : _target(&target), _previous(std::exchange(target._calls, this)) {}

    PureWindow::Call::~Call() {
        if (!_target)
            return;
        auto** current = &_target->_calls;
        while (*current && *current != this)
            current = &(*current)->_previous;
        if (*current)
            *current = _previous;
    }

    std::string PureWindow::name() const {
        return _state ? _state->name : std::string{};
    }

    PureWindow::operator bool() const noexcept {
        return _state && !_state->closed && _state->window_handle;
    }

    bool PureWindow::operator==(const PureWindow& other) const noexcept {
        return _state.get() == other._state.get() && !_state.owner_before(other._state) && !other._state.owner_before(_state);
    }

    WeakWindow PureWindow::weak() const noexcept {
        return WeakWindow(_state);
    }

    WindowIdentity PureWindow::identity() const noexcept {
        return WindowIdentity(_state);
    }

    WindowIdentity::WindowIdentity(const std::shared_ptr<PureWindow::WindowData>& state) noexcept : _pointer(state.get()), _owner(state) {}

    bool WindowIdentity::operator==(const WindowIdentity& other) const noexcept {
        return _pointer == other._pointer && !_owner.owner_before(other._owner) && !other._owner.owner_before(_owner);
    }

    PureWindow WeakWindow::lock() const noexcept {
        auto state = _state.lock();
        return state && !state->closed && state->window_handle ? PureWindow(std::move(state)) : PureWindow{};
    }

    bool PureWindow::close() {
        const auto state = _state;
        if (!state || !state->window_handle)
            return false;
        state->destroy();
        return true;
    }

    bool PureWindow::resize(int width, int height) {
        if (width <= 0 || height <= 0)
            throw std::invalid_argument("Window size must be positive");
        Call call(*this);
        const auto state = _state;
        if (!*this)
            return false;
        const auto owner = state->os_window;
        lifetime::GLFWCallScope scope;
        if (!call.active() || state->closed)
            return false;
        const auto native = owner.native_handle();
        operation::resize_os_window(native, width, height);

        // Window managerが新しいサイズを反映してから描画資源の再構築へ進む。
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        while (call.active() && !state->closed) {
            operation::poll_os_windows();
            if (!call.active() || state->closed)
                return false;
            if (operation::os_window_has_size(native, width, height))
                return true;
            if (std::chrono::steady_clock::now() >= deadline)
                return false;
            operation::wait_os_window_events(0.01);
        }
        return false;
    }

    lifetime::OSWindowOwner PureWindow::window_owner() const {
        if (!*this || !_state->os_window)
            throw exceptions::RuntimeException("Window", "A live managed window is required");
        return _state->os_window;
    }

    bool PureWindow::should_close() const {
        const auto state = _state;
        if (!*this)
            return false;
        const auto owner = state->os_window;
        lifetime::GLFWCallScope scope;
        return !state->closed && operation::os_window_should_close(owner.native_handle());
    }

    void PureWindow::set_should_close(bool value) {
        const auto state = _state;
        const auto owner = window_owner();
        lifetime::GLFWCallScope scope;
        if (!state->closed)
            operation::set_os_window_should_close(owner.native_handle(), value);
    }

    Window::Window(std::string name, int width, int height) : PureWindow(std::move(name), width, height) {
        try {
            add_plugin<window_plugin::QuietClose>();
        } catch (...) {
            close();
            throw;
        }
    }

    Window::Window(PureWindow source) : PureWindow(std::move(source)) {
        if (*this && !plugin(window_plugin::QuietClose::QUIET_CLOSE_USED_ID))
            add_plugin<window_plugin::QuietClose>();
    }
} // namespace VSL_NAMESPACE
