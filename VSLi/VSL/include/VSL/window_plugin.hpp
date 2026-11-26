#pragma once

#include <utility>
#include <functional>

#include "window.hpp"

namespace VSL_NAMESPACE::window_plugin {
    struct QuietClose : public VSL_NAMESPACE::PureWindow::Plugin, public VSL_NAMESPACE::PureWindow::OnUpdate {
        constexpr static unsigned char QUIET_CLOSE_USED_ID = -1;

        QuietClose(VSL_NAMESPACE::PureWindow*);
        // OnUpdate
        bool on_update(VSL_NAMESPACE::PureWindow data) override;
    };

    struct HookShouldClose : public VSL_NAMESPACE::PureWindow::Plugin, public VSL_NAMESPACE::PureWindow::OnUpdate {
        HookShouldClose(PureWindow*, std::function<void(PureWindow)> callback) : callback(std::move(callback)) {}

        // OnUpdate
        bool on_update(VSL_NAMESPACE::PureWindow data) override;

        std::function<void(PureWindow)> callback;
    };

    struct HookResize : public VSL_NAMESPACE::PureWindow::Plugin {
        constexpr static unsigned char HOOK_RESIZE_USED_ID = -2;

        HookResize(VSL_NAMESPACE::PureWindow*, std::function<void(PureWindow, int, int)> callback);

        std::function<void(PureWindow, int, int)> callback;
    };
} // namespace VSL_NAMESPACE::window_plugin
