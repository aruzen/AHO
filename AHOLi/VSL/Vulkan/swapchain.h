#pragma once
#include "../define.h"
#include "pv.h"

#include "device.h"

namespace VSL_NAMESPACE {
	struct SwapchainAcsessor {
		std::shared_ptr<_impl::Swapchain_impl> _data;
	};

	template<bool V = VSL_NAMESPACE::validation>
	struct Swapchain : public SwapchainAcsessor {
		Swapchain(VSL_NAMESPACE::LogicalDeviceAcsessor device);
	};
}