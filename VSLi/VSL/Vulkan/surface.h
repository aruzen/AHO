#pragma once
#include "../define.h"
#include "pv.h"
#include "../debug.h"

#include "../Window.h"
#include "Vulkan.h"

namespace VSL_NAMESPACE {
	struct Surface : public vsl::PureWindow::Plugin {
		Surface(vsl::PureWindow* data, VSL_NAMESPACE::VulkanAccessor instance);

		std::shared_ptr<vsl::_impl::Surface_impl> _data;
	};
}