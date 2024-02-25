#include "pch.h"
#include "../define.h"

#include "_pimpls.h"
#include "../exceptions.h"

#include "Vulkan.h"
#include "command.h"

VSL_NAMESPACE::CommandManager::CommandManager(VSL_NAMESPACE::VulkanAccessor vk, VSL_NAMESPACE::LogicalDeviceAccessor device)
{
	_data = std::shared_ptr<VSL_NAMESPACE::_impl::CommandManager_impl>(new VSL_NAMESPACE::_impl::CommandManager_impl);
	vk::Device de = device._data->device;
	_data->graphicsQueue = de.getQueue(device._data->graphicsFamily, 0);
	_data->presentQueuea = de.getQueue(device._data->presentFamily, 0);
}