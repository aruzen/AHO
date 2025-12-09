#pragma once
#include "../define.hpp"
#include "pv.hpp"
#include "../utils/string.hpp"

#include "../window.hpp"
#include "vulkan.hpp"

namespace VSL_NAMESPACE {
	struct Surface : public vsl::PureWindow::Plugin {
		Surface(vsl::PureWindow* data, VSL_NAMESPACE::VulkanAccessor instance);

		std::shared_ptr<vsl::_impl::Surface_impl> _data;
	};
}