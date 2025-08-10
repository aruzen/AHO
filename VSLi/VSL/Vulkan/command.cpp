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

VSL_NAMESPACE::CommandPool::CommandPool(std::shared_ptr<VSL_NAMESPACE::_impl::CommandPool_impl> data) : _data(data) {}

VSL_NAMESPACE::CommandBuffer::CommandBuffer(VSL_NAMESPACE::CommandPool pool, std::uint32_t size)
{
	_data = std::shared_ptr<VSL_NAMESPACE::_impl::CommandBuffer_impl>(new VSL_NAMESPACE::_impl::CommandBuffer_impl);
	_data->commandPool = pool._data;

	_data->commandBuffers.resize(size);
	vk::CommandBufferAllocateInfo allocInfo;
	allocInfo.commandPool = pool._data->commandPool;
	allocInfo.level = vk::CommandBufferLevel::ePrimary;
	allocInfo.commandBufferCount = size;

	_data->currentBufferIdx = 0;
	_data->commandBuffers = pool._data->device->device.allocateCommandBuffers(allocInfo);
}

VSL_NAMESPACE::CommandBuffer::CommandBuffer(std::shared_ptr<VSL_NAMESPACE::_impl::CommandBuffer_impl> data) : _data(data) {}

std::uint32_t VSL_NAMESPACE::CommandBuffer::getCurrentBufferIdx() {
	return _data->currentBufferIdx;
}

size_t VSL_NAMESPACE::CommandBuffer::getSize() {
	return _data->commandBuffers.size();
}

void VSL_NAMESPACE::CommandBuffer::next()
{
	_data->currentBufferIdx = (_data->currentBufferIdx + 1) % getSize();
}

void VSL_NAMESPACE::CommandBuffer::reset()
{
	for (auto& buff : _data->commandBuffers)
		buff.reset();
}

void VSL_NAMESPACE::CommandBuffer::reset(std::uint32_t idx)
{
	_data->commandBuffers[idx].reset();
}

VSL_NAMESPACE::_impl::CommandBuffer_impl::~CommandBuffer_impl() {
	commandPool->device->device.freeCommandBuffers(commandPool->commandPool, this->commandBuffers);
}

VSL_NAMESPACE::CommandManager::CommandManager(std::shared_ptr<VSL_NAMESPACE::_impl::CommandManager_impl> data) : _data(data) {}

VSL_NAMESPACE::CommandManager::CommandManager(VSL_NAMESPACE::LogicalDeviceAccessor device)
{
	_data = std::shared_ptr<VSL_NAMESPACE::_impl::CommandManager_impl>(new VSL_NAMESPACE::_impl::CommandManager_impl);
	_data->device = device._data;
	vk::Device de = device._data->device;
	CommandPool pool(device);
	CommandBuffer buffer(pool);

	_data->commandPool = pool._data;
	_data->commandBuffer = buffer._data;
	_data->graphicsQueue = de.getQueue(device._data->graphicsFamily, 0);
	_data->presentQueue = de.getQueue(device._data->presentFamily, 0);
}

VSL_NAMESPACE::CommandPool VSL_NAMESPACE::CommandManager::getPool()
{
	return CommandPool{ _data->commandPool };
}

VSL_NAMESPACE::CommandBuffer VSL_NAMESPACE::CommandManager::getBuffer()
{
	return CommandBuffer{ _data->commandBuffer };
}

std::uint32_t VSL_NAMESPACE::CommandManager::getCurrentBufferIdx()
{
	return _data->commandBuffer->currentBufferIdx;
}

VSL_NAMESPACE::CommandBuffer VSL_NAMESPACE::CommandManager::makeExclusiveBuffer(size_t size)
{
	return CommandBuffer(CommandPool{ _data->commandPool }, size);
}

void VSL_NAMESPACE::CommandManager::next() {
	CommandBuffer{ _data->commandBuffer }.next();
}

VSL_NAMESPACE::CommandManager VSL_NAMESPACE::CommandManager::setDefault()
{
	auto before_manager = defaults::COMMAND_MANAGER;
	defaults::COMMAND_MANAGER = _data;
	return VSL_NAMESPACE::CommandManager{ before_manager };
}
