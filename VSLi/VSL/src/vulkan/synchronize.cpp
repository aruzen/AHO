#include <VSL/vulkan/synchronize.hpp>

#include <VSL/vulkan/_pimpls.hpp>
#include <VSL/exceptions.hpp>

/*
 * FIXME
 * おそらくValidationLayer側のバグであるがで1回目のreset時のみエラーが出る
 *  frame #0: 0x0000000181ba4158 libc++abi.dylib`__cxa_throw
 *  frame #1: 0x0000000181b14a4c libc++.1.dylib`std::exception_ptr std::make_exception_ptr[abi:ue170006]<std::__1::future_error>(std::__1::future_error) + 104
 *  frame #2: 0x0000000181b14978 libc++.1.dylib`std::__1::promise<void>::~promise() + 112
 *  frame #3: 0x0000000108843cc4 libVkLayer_khronos_validation.dylib`vvl::Fence::Reset() + 124
 */
#include <future>
#include <source_location>

void VSL_NAMESPACE::SemaphoreHolder::destroy() {
    if (_data == nullptr)
        return;
    auto manager = _data->_manager;
    if (manager != nullptr && manager->device != nullptr) {
        for (auto semaphore : _data->semaphores)
            if (semaphore)
                manager->device->device.destroySemaphore(semaphore);
    }
    _data->semaphores.clear();
    if (manager != nullptr) {
        for (auto it = manager->semaphores.begin(); it != manager->semaphores.end();) {
            if (it->second == _data)
                it = manager->semaphores.erase(it);
            else
                ++it;
        }
    }
}


bool VSL_NAMESPACE::FenceHolder::wait(std::uint64_t timeout_ns) {
    if (_data == nullptr || _data->_manager == nullptr || _data->_manager->device == nullptr) {
        throw VSL_NAMESPACE::exceptions::RuntimeException(
                "Fence",
                "wait called on an invalid fence holder.",
                std::source_location::current());
    }
	if (vk::Result::eSuccess == _data->_manager->device->device.waitForFences(_data->fences, true, timeout_ns))
		return true;
	return false;
}

bool VSL_NAMESPACE::FenceHolder::wait(std::uint32_t target_idx, std::uint64_t timeout_ns)
{
    if (_data == nullptr || _data->_manager == nullptr || _data->_manager->device == nullptr || target_idx >= _data->fences.size()) {
        throw VSL_NAMESPACE::exceptions::RuntimeException(
                "Fence",
                "wait called on an invalid fence holder or index.",
                std::source_location::current());
    }
	if (vk::Result::eSuccess == _data->_manager->device->device.waitForFences({ _data->fences[target_idx] }, true, timeout_ns))
		return true;
	return false;
}

void VSL_NAMESPACE::FenceHolder::reset() {
    if (_data == nullptr || _data->_manager == nullptr || _data->_manager->device == nullptr) {
        throw VSL_NAMESPACE::exceptions::RuntimeException(
                "Fence",
                "reset called on an invalid fence holder.",
                std::source_location::current());
    }
	_data->_manager->device->device.resetFences(_data->fences);
}

void VSL_NAMESPACE::FenceHolder::reset(std::uint32_t target_idx)
{
    if (_data == nullptr || _data->_manager == nullptr || _data->_manager->device == nullptr || target_idx >= _data->fences.size()) {
        throw VSL_NAMESPACE::exceptions::RuntimeException(
                "Fence",
                "reset called on an invalid fence holder or index.",
                std::source_location::current());
    }
    try {
        auto result = _data->_manager->device->device.resetFences(1, &_data->fences[target_idx]);
        if (result != vk::Result::eSuccess)
            loggingln("resetFence failed.");
    } catch (...) {
        loggingln("Exception caught in resetFences");
    }
}

void VSL_NAMESPACE::FenceHolder::destroy() {
    if (_data == nullptr)
        return;
    auto manager = _data->_manager;
    if (manager != nullptr && manager->device != nullptr) {
        for (auto fence : _data->fences)
            if (fence)
                manager->device->device.destroyFence(fence);
    }
    _data->fences.clear();
    if (manager != nullptr) {
        for (auto it = manager->fences.begin(); it != manager->fences.end();) {
            if (it->second == _data)
                it = manager->fences.erase(it);
            else
                ++it;
        }
    }
}

VSL_NAMESPACE::SynchroManager::SynchroManager(LogicalDeviceAccessor device) {
    if (device._data == nullptr) {
        throw VSL_NAMESPACE::exceptions::RuntimeException(
                "SynchroManager",
                "SynchroManager creation requires a valid logical device.",
                std::source_location::current());
    }
	_data = std::make_shared<_impl::SynchroManager_impl>();
	_data->device = device._data;
}

VSL_NAMESPACE::SemaphoreHolder VSL_NAMESPACE::SynchroManager::createSemaphore(std::string name)
{
    if (_data == nullptr || _data->device == nullptr) {
        throw VSL_NAMESPACE::exceptions::RuntimeException(
                "SynchroManager",
                "createSemaphore requires a valid logical device.",
                std::source_location::current());
    }
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
    if (_data == nullptr || _data->device == nullptr) {
        throw VSL_NAMESPACE::exceptions::RuntimeException(
                "SynchroManager",
                "createSemaphore requires a valid logical device.",
                std::source_location::current());
    }
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
    if (_data == nullptr || _data->device == nullptr) {
        throw VSL_NAMESPACE::exceptions::RuntimeException(
                "SynchroManager",
                "createFence requires a valid logical device.",
                std::source_location::current());
    }
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
    if (_data == nullptr || _data->device == nullptr) {
        throw VSL_NAMESPACE::exceptions::RuntimeException(
                "SynchroManager",
                "createFence requires a valid logical device.",
                std::source_location::current());
    }
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
	return true;
}

bool VSL_NAMESPACE::SynchroManager::destroyFence(std::string name)
{
	if (not _data->fences.contains(name))
		return false;
	FenceHolder{ _data->fences[name] }.destroy();
	return true;
}

std::optional<VSL_NAMESPACE::FenceHolder> VSL_NAMESPACE::SynchroManager::operator[](std::string name)
{
	return this->getFence(name);
}

VSL_NAMESPACE::_impl::SynchroManager_impl::~SynchroManager_impl() {
	while (!semaphores.empty())
		SemaphoreHolder{ semaphores.begin()->second }.destroy();
	while (!fences.empty())
		FenceHolder{ fences.begin()->second }.destroy();
}
