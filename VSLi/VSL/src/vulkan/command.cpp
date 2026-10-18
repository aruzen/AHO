#include <VSL/define.hpp>

#include <VSL/vulkan/_pimpls.hpp>
#include <VSL/exceptions.hpp>

#include <VSL/vulkan/vulkan.hpp>
#include <VSL/vulkan/command.hpp>
#include <VSL/vulkan/phase.hpp>
#include <VSL/vulkan/surface.hpp>


VSL_NAMESPACE::CommandPool::CommandPool(VSL_NAMESPACE::LogicalDeviceAccessor device)
{
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
	_data->currentBufferIdx = (_data->currentBufferIdx + 1) % _data->commandBuffers.size();
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
    for (const auto &submission : submissions)
        if (submission && submission->status == Completion::Status::Submitted)
            Completion{submission}.wait();
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
void vsl::CommandManager::waitIdle() { _data->device->device.waitIdle(); }

vsl::_impl::CommandPool_impl::~CommandPool_impl() {
    if (commandPool) device->device.destroyCommandPool(commandPool);
}

vsl::LogicalDeviceAccessor vsl::CommandManager::getDevice() const {
    if (!_data || !_data->device || !_data->device->device)
        throw std::invalid_argument("Command manager is empty");
    return {_data->device};
}
vsl::CommandManager vsl::CommandManager::independent(std::uint32_t count) const {
    if (!count) throw std::invalid_argument("Command buffer count must be positive");
    CommandPool pool(getDevice());
    CommandBuffer buffers(pool, count);
    auto next = std::make_shared<_impl::CommandManager_impl>();
    next->device = _data->device;
    next->commandPool = pool._data;
    next->commandBuffer = buffers._data;
    next->graphicsQueue = _data->graphicsQueue;
    next->presentQueue = _data->presentQueue;
    return CommandManager(next);
}
