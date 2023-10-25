#pragma once

#include "Window.h"

namespace vsl::window_plugin {
	struct ShouldClose : public vsl::PureWindow::Plugin, public vsl::PureWindow::OnUpdateable {
		ShouldClose(vsl::PureWindow::WindowData*) {};
		// OnUpdateable ‚ğ‰î‚µ‚ÄŒp³‚³‚ê‚Ü‚µ‚½
		virtual bool onUpdate(vsl::PureWindow::WindowData* data) override;
	};
}