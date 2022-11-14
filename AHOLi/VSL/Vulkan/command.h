#pragma once
#include "../define.h"

#include "device.h"

namespace VSL_NAMESPACE {
	template<bool V = vsl::validation>
	struct CommandManager {
		CommandManager(VSL_NAMESPACE::Vulkan<V> vk, VSL_NAMESPACE::LogicalDeviceAcsessor device);

		std::shared_ptr<VSL_NAMESPACE::_impl::CommandManager_impl> _data;
	};
}