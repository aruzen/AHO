#pragma once
#include "window_state.hpp"

namespace VSL_NAMESPACE::lifetime::detail {
    struct WindowAccess {
        static const std::shared_ptr<PureWindow::WindowData>& State(const PureWindow& window) noexcept { return window._state; }
        static std::shared_ptr<PureWindow::WindowData> State(PureWindow&& window) noexcept { return window._state; }
        static std::shared_ptr<PureWindow::WindowData> State(const PureWindow&& window) noexcept { return window._state; }
        static PureWindow FromState(std::shared_ptr<PureWindow::WindowData> state) noexcept { return PureWindow(std::move(state)); }

        static std::vector<std::shared_ptr<PureWindow::WindowData>>& Registry() noexcept;
        static PureWindow FindNative(void* native) noexcept;
    };
} // namespace VSL_NAMESPACE::lifetime::detail
