#pragma once
#include "../define.h"

#include "device.h"

namespace VSL_NAMESPACE {
	struct CommandManager {
		CommandManager(VSL_NAMESPACE::VulkanAccessor vk, VSL_NAMESPACE::LogicalDeviceAccessor device);

		std::shared_ptr<VSL_NAMESPACE::_impl::CommandManager_impl> _data;
	};
}