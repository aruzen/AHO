#pragma once

#include "Window.h"

namespace vsl::window_plugin {
	struct QuietClose : public vsl::PureWindow::Plugin, public vsl::PureWindow::OnUpdateable {
		QuietClose(vsl::PureWindow*) {};
		// OnUpdateable
		virtual bool onUpdate(vsl::PureWindow data) override;
	};

	struct HookShouldClose : public vsl::PureWindow::Plugin, public vsl::PureWindow::OnUpdateable {
		HookShouldClose(vsl::PureWindow::WindowData*, std::function<void(PureWindow)> callback) : callback(callback) {};
		// OnUpdateable
		virtual bool onUpdate(vsl::PureWindow data) override;

		std::function<void(PureWindow)> callback;
	};

	struct HookResize : public vsl::PureWindow::Plugin {
		const static unsigned char HOOK_RESIZE_USED_ID = -1;

		HookResize(vsl::PureWindow*, std::function<void(PureWindow, int, int)> callback);

		std::function<void(PureWindow, int, int)> callback;
	};
}