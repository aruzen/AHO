#ifdef _MSC_VER
#include "pch.h"
#endif
#include "../define.h"

#include "_pimpls.h"
#include "../exceptions.h"

#include "Vulkan.h"
#include "command.h"
#include "surface.h"

VSL_NAMESPACE::CommandPool::CommandPool(VSL_NAMESPACE::LogicalDeviceAccessor device)
{
	_data = std::shared_ptr<VSL_NAMESPACE::_impl::CommandPool_impl>(new VSL_NAMESPACE::_impl::CommandPool_impl);
	_data->device = device._data;

	auto queueFamilyIndices = VSL_NAMESPACE::_impl::helper::findQueueFamilies(device._data->parentDevice->device, device._data->parentSurface->surface);

	vk::CommandPoolCreateInfo poolInfo;
	poolInfo.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
	poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();

	_data->commandPool = device._data->device.createCommandPool(poolInfo);
}

VSL_NAMESPACE::CommandBuffer::CommandBuffer(VSL_NAMESPACE::CommandPool pool)
{
	_data = std::shared_ptr<VSL_NAMESPACE::_impl::CommandBuffer_impl>(new VSL_NAMESPACE::_impl::CommandBuffer_impl);
	_data->commandPool = pool._data;

	vk::CommandBufferAllocateInfo allocInfo;
	allocInfo.commandPool = pool._data->commandPool;
	allocInfo.level = vk::CommandBufferLevel::ePrimary;
	allocInfo.commandBufferCount = 1;

	_data->commandBuffers = pool._data->device->device.allocateCommandBuffers(allocInfo);
}


VSL_NAMESPACE::DefaultPhase::DefaultPhase(VSL_NAMESPACE::CommandPool pool, VSL_NAMESPACE::CommandBuffer buffer) : pool(pool), buffer(buffer) {
	vk::CommandBufferBeginInfo beginInfo;
	// Optional beginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit; 
	beginInfo.pInheritanceInfo = nullptr; // Optional

	// TODO                      ↓ Why
	buffer._data->commandBuffers[0].begin(beginInfo);
}

VSL_NAMESPACE::DefaultPhase& VSL_NAMESPACE::DefaultPhase::operator<<(std::shared_ptr<command::__Command> cmd)
{
	cmd->invoke(pool, buffer);

	return *this;
}

VSL_NAMESPACE::DefaultPhase::~DefaultPhase() {
	// TODO                      ↓ Why
	buffer._data->commandBuffers[0].end();
}


VSL_NAMESPACE::CommandManager::CommandManager(VSL_NAMESPACE::LogicalDeviceAccessor device) : pool(device), buffer(pool)
{
	_data = std::shared_ptr<VSL_NAMESPACE::_impl::CommandManager_impl>(new VSL_NAMESPACE::_impl::CommandManager_impl);
	vk::Device de = device._data->device;
	// _data->graphicsQueue = de.getQueue(device._data->graphicsFamily, 0);
	// _data->presentQueuea = de.getQueue(device._data->presentFamily, 0);
}


