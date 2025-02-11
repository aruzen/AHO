#include "pch.h"
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

VSL_NAMESPACE::DefaultPhase::DefaultPhase(CommandManager _manager, SwapchainAccessor swapchain,
	std::optional<SemaphoreHolder> nextImageAvailable,
	std::optional<SemaphoreHolder> calculationFinish,
	std::optional<FenceHolder> inFlightFence)
	: manager(_manager)
	, swapchain(swapchain)
	, nextImageAvailable(nextImageAvailable)
	, calculationFinish(calculationFinish)
	, inFlightFence(inFlightFence) {
	auto&& manager = this->manager._data;
	auto&& currentFrame = manager->commandBuffer->currentBufferIdx;

	if (inFlightFence.has_value()) {
		inFlightFence.value().wait(currentFrame);
		inFlightFence.value().reset(currentFrame);
	}

	// Fix: Result check
	if (nextImageAvailable.has_value())
		imageIndex = manager->device->device.acquireNextImageKHR(swapchain._data->swapChain, UINT64_MAX
			, nextImageAvailable.value()._data->
			semaphores[currentFrame],
			nullptr).value;
	else
		imageIndex = manager->device->device.acquireNextImageKHR(swapchain._data->swapChain, UINT64_MAX, nullptr, nullptr).value;

	vk::CommandBufferBeginInfo beginInfo;
	// Optional beginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit; 
	beginInfo.pInheritanceInfo = nullptr; // Optional

	manager->commandBuffer->commandBuffers[currentFrame].reset();
	manager->commandBuffer->commandBuffers[currentFrame].begin(beginInfo);
}

VSL_NAMESPACE::DefaultPhaseStreamOperator VSL_NAMESPACE::DefaultPhase::operator<<(std::shared_ptr<command::__Command> cmd)
{
	DefaultPhaseStreamOperator op{ this };
	op << cmd;
	return op;
}

VSL_NAMESPACE::DefaultPhaseStreamOperator& VSL_NAMESPACE::DefaultPhaseStreamOperator::operator<<(std::shared_ptr<command::__Command> cmd)
{
	cmd->invoke(parent->manager.getPool(), parent->manager.getBuffer(), parent->manager);
	if (auto vholder = std::dynamic_pointer_cast<command::__VertexHolder>(cmd); vholder)
		this->vertexSize = vholder->get_vertex_size();
	return *this;
}

VSL_NAMESPACE::DefaultPhaseStreamOperator& VSL_NAMESPACE::DefaultPhaseStreamOperator::operator<<(std::shared_ptr<command::__Manipulator> manip)
{
	manip->manipulate(this, parent->manager.getPool(), parent->manager.getBuffer(), parent->manager);
	if (auto vholder = std::dynamic_pointer_cast<command::__VertexHolder>(manip); vholder)
		this->vertexSize = vholder->get_vertex_size();
	return *this;
}

std::uint32_t VSL_NAMESPACE::DefaultPhase::getImageIndex() {
	return imageIndex;
}


VSL_NAMESPACE::DefaultPhase::~DefaultPhase() {
	auto&& manager = this->manager._data;
	auto&& currentFrame = manager->commandBuffer->currentBufferIdx;

	manager->commandBuffer->commandBuffers[currentFrame].end();

	std::uint32_t imageIdx = getImageIndex();
	
	std::vector<vk::Semaphore> waitSemaphores, signalSemaphores;
	if (nextImageAvailable.has_value())
		waitSemaphores.push_back(nextImageAvailable.value()._data->semaphores[currentFrame]);
	if (calculationFinish.has_value())
		signalSemaphores.push_back(calculationFinish.value()._data->semaphores[currentFrame]);

	vk::PipelineStageFlags waitStages[] = { vk::PipelineStageFlagBits::eColorAttachmentOutput };
	
	vk::SubmitInfo submitInfo;
	submitInfo.waitSemaphoreCount = (std::uint32_t)waitSemaphores.size();
	submitInfo.pWaitSemaphores = waitSemaphores.data();
	submitInfo.pWaitDstStageMask = waitStages;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &manager->commandBuffer->commandBuffers[currentFrame];
	submitInfo.signalSemaphoreCount = signalSemaphores.size();
	submitInfo.pSignalSemaphores = signalSemaphores.data();

	if (inFlightFence.has_value())
		manager->graphicsQueue.submit({ submitInfo }, inFlightFence.value()._data->fences[currentFrame]);
	else
		manager->graphicsQueue.submit({ submitInfo });

	vk::PresentInfoKHR presentInfo;
	vk::SwapchainKHR swapchains[] = { swapchain._data->swapChain };

	presentInfo.waitSemaphoreCount = signalSemaphores.size();
	presentInfo.pWaitSemaphores = signalSemaphores.data();
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = swapchains;
	presentInfo.pImageIndices = &imageIdx;
	presentInfo.pResults = nullptr;

	manager->presentQueue.presentKHR(presentInfo);
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

void VSL_NAMESPACE::CommandManager::next() {
	CommandBuffer{ _data->commandBuffer }.next();
}