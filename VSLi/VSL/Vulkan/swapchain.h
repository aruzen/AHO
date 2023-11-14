#pragma once
#include "../define.h"
#include "pv.h"

#include "device.h"

namespace VSL_NAMESPACE {
	struct SwapchainAccessor {
		std::shared_ptr<_impl::Swapchain_impl> _data;
	};

	template<bool V = VSL_NAMESPACE::validation>
	struct Swapchain : public SwapchainAccessor {
		Swapchain(VSL_NAMESPACE::LogicalDeviceAccessor device);
	};
}