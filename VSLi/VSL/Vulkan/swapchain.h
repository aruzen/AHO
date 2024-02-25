#pragma once
#include "../define.h"
#include "pv.h"

#include "device.h"

namespace VSL_NAMESPACE {
	struct SwapchainAccessor {
		std::shared_ptr<_impl::Swapchain_impl> _data;
	};

	struct Swapchain : public SwapchainAccessor {
		Swapchain(LogicalDeviceAccessor device);
	};
}