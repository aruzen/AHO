#pragma once
#include "../define.h"
#include "pv.h"
#include "../debug.h"

#include "../Window.h"
#include "Vulkan.h"

namespace VSL_NAMESPACE {
	template< bool Validation>
	struct Surface : public vsl::PureWindow::Plugin {
		Surface(vsl::PureWindow::WindowData * data, VSL_NAMESPACE::Vulkan<Validation> instance);

		std::shared_ptr<vsl::_impl::Surface_impl> _data;
	};
}