#include <VSL/define.hpp>

#include <VSL/vulkan/_pimpls.hpp>
#include <VSL/exceptions.hpp>

#include <VSL/vulkan/vulkan.hpp>
#include <VSL/vulkan/command.hpp>
#include <VSL/vulkan/surface.hpp>

#include <source_location>

VSL_NAMESPACE::CommandPool::CommandPool(VSL_NAMESPACE::LogicalDeviceAccessor device)
{
    if (device._data == nullptr) {
        throw VSL_NAMESPACE::exceptions::RuntimeException(
                "CommandPool",
                "CommandPool creation requires a valid logical device.",
                std::source_location::current());
    }
	_data = std::shared_ptr<VSL_NAMESPACE::_impl::CommandPool_impl>(new VSL_NAMESPACE::_impl::CommandPool_impl);
	_data->device = device._data;

	vk::CommandPoolCreateInfo poolInfo;
	poolInfo.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
	poolInfo.queueFamilyIndex = device._data->graphicsFamily;

	_data->commandPool = device._data->device.createCommandPool(poolInfo);
}

VSL_NAMESPACE::CommandPool::CommandPool(std::shared_ptr<VSL_NAMESPACE::_impl::CommandPool_impl> data) : _data(data) {}

VSL_NAMESPACE::CommandBuffer::CommandBuffer(VSL_NAMESPACE::CommandPool pool, std::uint32_t size)
{
    if (pool._data == nullptr || pool._data->device == nullptr || !pool._data->commandPool) {
        throw VSL_NAMESPACE::exceptions::RuntimeException(
                "CommandBuffer",
                "CommandBuffer creation requires a valid command pool.",
                std::source_location::current());
    }
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
    if (_data == nullptr)
        throw VSL_NAMESPACE::exceptions::RuntimeException("CommandBuffer", "Invalid command buffer.", std::source_location::current());
	return _data->currentBufferIdx;
}

size_t VSL_NAMESPACE::CommandBuffer::getSize() {
    if (_data == nullptr)
        throw VSL_NAMESPACE::exceptions::RuntimeException("CommandBuffer", "Invalid command buffer.", std::source_location::current());
	return _data->commandBuffers.size();
}

void VSL_NAMESPACE::CommandBuffer::reset()
{
    if (_data == nullptr)
        throw VSL_NAMESPACE::exceptions::RuntimeException("CommandBuffer", "Invalid command buffer.", std::source_location::current());
	for (auto& buff : _data->commandBuffers)
		buff.reset();
}

void VSL_NAMESPACE::CommandBuffer::reset(std::uint32_t idx)
{
    if (_data == nullptr || idx >= _data->commandBuffers.size())
        throw VSL_NAMESPACE::exceptions::RuntimeException("CommandBuffer", "Invalid command buffer index.", std::source_location::current());
	_data->commandBuffers[idx].reset();
}

VSL_NAMESPACE::_impl::CommandBuffer_impl::~CommandBuffer_impl() {
    if (commandPool != nullptr && commandPool->device != nullptr && commandPool->commandPool && !commandBuffers.empty())
	    commandPool->device->device.freeCommandBuffers(commandPool->commandPool, this->commandBuffers);
}

VSL_NAMESPACE::_impl::CommandPool_impl::~CommandPool_impl() {
    if (device != nullptr && commandPool)
        device->device.destroyCommandPool(commandPool);
}

VSL_NAMESPACE::CommandManager::CommandManager(std::shared_ptr<VSL_NAMESPACE::_impl::CommandManager_impl> data) : _data(data) {}

VSL_NAMESPACE::CommandManager::CommandManager(VSL_NAMESPACE::LogicalDeviceAccessor device)
{
    if (device._data == nullptr) {
        throw VSL_NAMESPACE::exceptions::RuntimeException(
                "CommandManager",
                "CommandManager creation requires a valid logical device.",
                std::source_location::current());
    }
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
    if (_data == nullptr || _data->commandPool == nullptr)
        throw VSL_NAMESPACE::exceptions::RuntimeException("CommandManager", "Invalid command pool.", std::source_location::current());
	return CommandPool{ _data->commandPool };
}

VSL_NAMESPACE::CommandBuffer VSL_NAMESPACE::CommandManager::getBuffer()
{
    if (_data == nullptr || _data->commandBuffer == nullptr)
        throw VSL_NAMESPACE::exceptions::RuntimeException("CommandManager", "Invalid command buffer.", std::source_location::current());
	return CommandBuffer{ _data->commandBuffer };
}

std::uint32_t VSL_NAMESPACE::CommandManager::getCurrentBufferIdx()
{
    if (_data == nullptr || _data->commandBuffer == nullptr)
        throw VSL_NAMESPACE::exceptions::RuntimeException("CommandManager", "Invalid command buffer.", std::source_location::current());
	return _data->commandBuffer->currentBufferIdx;
}

VSL_NAMESPACE::CommandBuffer VSL_NAMESPACE::CommandManager::makeExclusiveBuffer(size_t size)
{
    if (_data == nullptr || _data->commandPool == nullptr)
        throw VSL_NAMESPACE::exceptions::RuntimeException("CommandManager", "Invalid command pool.", std::source_location::current());
	return CommandBuffer(CommandPool{ _data->commandPool }, size);
}
