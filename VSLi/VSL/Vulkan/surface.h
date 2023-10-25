#pragma once
#include "../define.h"
#include "pv.h"
#include "../debug.h"

#include "../Window.h"
#include "Vulkan.h"

namespace VSL_NAMESPACE {
	struct SurfaceAccessor {
		std::shared_ptr<vsl::_impl::Surface_impl> _data;
	};

	template< bool Validation>
	struct Surface : public vsl::PureWindow::Plugin, public SurfaceAccessor {
		Surface(vsl::PureWindow::WindowData * data, VSL_NAMESPACE::Vulkan<Validation> instance);
	};
}