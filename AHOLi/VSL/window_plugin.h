#pragma once

#include "Window.h"

namespace vsl::window_plugin {
	struct ShouldClose : public vsl::PureWindow::Plugin, public vsl::PureWindow::OnUpdateable {
		ShouldClose(vsl::PureWindow::WindowData*) {};
		// OnUpdateable を介して継承されました
		virtual bool onUpdate(vsl::PureWindow::WindowData* data) override;
	};
}