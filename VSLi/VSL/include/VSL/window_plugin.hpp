#pragma once

#include <utility>

#include "window.hpp"

namespace vsl::window_plugin {
	struct QuietClose : public vsl::PureWindow::Plugin, public vsl::PureWindow::OnUpdate {
        constexpr static unsigned char QUIET_CLOSE_USED_ID = -1;

		QuietClose(vsl::PureWindow*);
		// OnUpdate
		bool onUpdate(vsl::PureWindow data) override;
	};

	struct HookShouldClose : public vsl::PureWindow::Plugin, public vsl::PureWindow::OnUpdate {
		HookShouldClose(vsl::PureWindow::WindowData*, std::function<void(PureWindow)> callback) : callback(std::move(callback)) {};
		// OnUpdate
		bool onUpdate(vsl::PureWindow data) override;

		std::function<void(PureWindow)> callback;
	};

	struct HookResize : public vsl::PureWindow::Plugin {
		constexpr static unsigned char HOOK_RESIZE_USED_ID = -2;

		HookResize(vsl::PureWindow*, std::function<void(PureWindow, int, int)> callback);

		std::function<void(PureWindow, int, int)> callback;
	};
}