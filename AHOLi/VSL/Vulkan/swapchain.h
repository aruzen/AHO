#pragma once
#include "../define.h"
#include "pv.h"

#include "device.h"

namespace VSL_NAMESPACE {
	template<bool V = VSL_NAMESPACE::validation>
	struct Swapchain {
		Swapchain(VSL_NAMESPACE::LogicalDeviceAcsessor device);

		std::shared_ptr<_impl::Swapchain_impl> _data;
	};
}