#include "pch.h"
#include "synchronize.h"

#include "_pimpls.h"


void VSL_NAMESPACE::SemaphoreHolder::destroy() {
	for (auto semaphore : _data->semaphores)
		_data->_manager->device->device.destroySemaphore(semaphore);
	for (const auto& [name, s] : _data->_manager->semaphores)
		if (s == _data)
			_data->_manager->semaphores.erase(name);
}


bool VSL_NAMESPACE::FenceHolder::wait(std::uint64_t timeout_ns) {
	if (vk::Result::eSuccess == _data->_manager->device->device.waitForFences(_data->fences, true, timeout_ns))
		return true;
	return false;
}

bool VSL_NAMESPACE::FenceHolder::wait(std::uint32_t target_idx, std::uint64_t timeout_ns)
{
	if (vk::Result::eSuccess == _data->_manager->device->device.waitForFences({ _data->fences[target_idx] }, true, timeout_ns))
		return true;
	return false;
}

void VSL_NAMESPACE::FenceHolder::reset() {
	_data->_manager->device->device.resetFences(_data->fences);
}

void VSL_NAMESPACE::FenceHolder::reset(std::uint32_t target_idx)
{
	_data->_manager->device->device.resetFences(_data->fences[target_idx]);
}

void VSL_NAMESPACE::FenceHolder::destroy() {
	for (auto fence : _data->fences)
		_data->_manager->device->device.destroyFence(fence);
	for (const auto& [name, f] : _data->_manager->fences)
		if (f == _data)
			_data->_manager->fences.erase(name);
}

VSL_NAMESPACE::SynchroManager::SynchroManager(LogicalDeviceAccessor device) {
	_data = std::make_shared<_impl::SynchroManager_impl>();
	_data->device = device._data;
}

VSL_NAMESPACE::SemaphoreHolder VSL_NAMESPACE::SynchroManager::createSemaphore(std::string name)
{
	std::shared_ptr<_impl::Semaphore_impl> data = std::make_shared<_impl::Semaphore_impl>();
	data->_manager = _data;

	vk::SemaphoreCreateInfo info;
	data->semaphores = { _data->device->device.createSemaphore(info) };
	data->semaphores.shrink_to_fit();

	_data->semaphores[name] = data;

	return SemaphoreHolder{ data };
}

VSL_NAMESPACE::SemaphoreHolder VSL_NAMESPACE::SynchroManager::createSemaphore(std::string name, size_t amount)
{
	std::shared_ptr<_impl::Semaphore_impl> data = std::make_shared<_impl::Semaphore_impl>();
	data->_manager = _data;

	vk::SemaphoreCreateInfo info;
	data->semaphores.resize(amount);
	for (size_t i = 0; i < amount; i++)
		data->semaphores[i] = _data->device->device.createSemaphore(info);
	data->semaphores.shrink_to_fit();

	_data->semaphores[name] = data;

	return SemaphoreHolder{ data };
}

VSL_NAMESPACE::FenceHolder VSL_NAMESPACE::SynchroManager::createFence(std::string name, bool signal)
{
	std::shared_ptr<_impl::Fence_impl> data = std::make_shared<_impl::Fence_impl>();;
	data->_manager = _data;

	vk::FenceCreateInfo info;
	if (signal)
		info.flags = vk::FenceCreateFlagBits::eSignaled;
	data->fences = { _data->device->device.createFence(info) };
	data->fences.shrink_to_fit();

	_data->fences[name] = data;

	return FenceHolder{ data };
}

VSL_NAMESPACE::FenceHolder VSL_NAMESPACE::SynchroManager::createFence(std::string name, size_t amount, bool signal)
{
	std::shared_ptr<_impl::Fence_impl> data = std::make_shared<_impl::Fence_impl>();;
	data->_manager = _data;

	vk::FenceCreateInfo info;
	if (signal)
		info.flags = vk::FenceCreateFlagBits::eSignaled;

	data->fences.resize(amount);
	for (size_t i = 0; i < amount; i++)
		data->fences[i] = _data->device->device.createFence(info);
	data->fences.shrink_to_fit();

	_data->fences[name] = data;

	return FenceHolder{ data };
}

std::optional<VSL_NAMESPACE::SemaphoreHolder> VSL_NAMESPACE::SynchroManager::getSemaphore(std::string name)
{
	if (not _data->semaphores.contains(name))
		return std::nullopt;
	return { SemaphoreHolder{ _data->semaphores[name] } };
}

std::optional<VSL_NAMESPACE::FenceHolder> VSL_NAMESPACE::SynchroManager::getFence(std::string name)
{
	if (not _data->fences.contains(name))
		return std::nullopt;
	return { FenceHolder{ _data->fences[name] } };
}

bool VSL_NAMESPACE::SynchroManager::destroySemaphore(std::string name)
{
	if (not _data->semaphores.contains(name))
		return false;
	SemaphoreHolder{ _data->semaphores[name] }.destroy();
	_data->semaphores.erase(name);
	return true;
}

bool VSL_NAMESPACE::SynchroManager::destroyFence(std::string name)
{
	if (not _data->fences.contains(name))
		return false;
	FenceHolder{ _data->fences[name] }.destroy();
	_data->fences.erase(name);
	return true;
}

std::optional<VSL_NAMESPACE::FenceHolder> VSL_NAMESPACE::SynchroManager::operator[](std::string name)
{
	return this->getFence(name);
}

VSL_NAMESPACE::_impl::SynchroManager_impl::~SynchroManager_impl() {
	for (const auto& [_, s] : semaphores)
		SemaphoreHolder{ s }.destroy();
	for (const auto& [_, f] : fences)
		FenceHolder{ f }.destroy();
}