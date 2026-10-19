#include <VSL/vulkan/synchronize.hpp>
#include <VSL/vulkan/_pimpls.hpp>
#include <stdexcept>

namespace vsl {
    namespace {
        template<class Map, class T>
        void eraseHandle(Map &map, const std::shared_ptr<T> &target) {
            std::erase_if(map, [&](const auto &entry) { return entry.second == target; });
        }
        void requireFence(const std::shared_ptr<_impl::Fence_impl> &data) {
            if (!data || data->fences.empty())
                throw std::logic_error("Fence has been destroyed");
        }
        void rememberCompletion(const std::shared_ptr<_impl::Fence_impl> &data, std::size_t index) {
            if (index < data->submissions.size())
                if (auto submission = data->submissions[index].lock())
                    (void)Completion{submission}.status();
        }
        void rememberCompletions(const std::shared_ptr<_impl::Fence_impl> &data) {
            // 公開APIでフェンスをリセット・破棄する前に、以前の送信結果を保存する。
            for (std::size_t i = 0; i < data->submissions.size(); ++i)
                rememberCompletion(data, i);
        }
    }
    _impl::Semaphore_impl::~Semaphore_impl() {
        for (auto handle : semaphores)
            device->device.destroySemaphore(handle);
    }
    _impl::Fence_impl::~Fence_impl() {
        for (auto handle : fences)
            device->device.destroyFence(handle);
    }
    _impl::SynchroManager_impl::~SynchroManager_impl() = default;

    std::size_t SemaphoreHolder::size() const { return _data ? _data->semaphores.size() : 0; }
    void SemaphoreHolder::destroy() {
        if (!_data)
            return;
        auto data = _data; // マップから削除しても参照が無効にならないよう、共有ポインタをコピーする。
        if (auto manager = data->_manager.lock())
            eraseHandle(manager->semaphores, data);
        for (auto handle : data->semaphores)
            data->device->device.destroySemaphore(handle);
        data->semaphores.clear();
    }
    bool FenceHolder::wait(std::uint64_t timeout) {
        requireFence(_data);
        const bool ready = _data->device->device.waitForFences(_data->fences, true, timeout) == vk::Result::eSuccess;
        if (ready)
            rememberCompletions(_data);
        return ready;
    }
    bool FenceHolder::wait(std::uint32_t index, std::uint64_t timeout) {
        requireFence(_data);
        const bool ready = _data->device->device.waitForFences(_data->fences.at(index), true, timeout) == vk::Result::eSuccess;
        if (ready)
            rememberCompletion(_data, index);
        return ready;
    }
    void FenceHolder::reset() {
        requireFence(_data);
        rememberCompletions(_data);
        _data->device->device.resetFences(_data->fences);
    }
    void FenceHolder::reset(std::uint32_t index) {
        requireFence(_data);
        rememberCompletion(_data, index);
        _data->device->device.resetFences(_data->fences.at(index));
    }
    void FenceHolder::destroy() {
        if (!_data)
            return;
        auto data = _data;
        rememberCompletions(data);
        if (auto manager = data->_manager.lock())
            eraseHandle(manager->fences, data);
        for (auto handle : data->fences)
            data->device->device.destroyFence(handle);
        data->fences.clear();
        data->submissions.clear();
    }
    SynchroManager::SynchroManager(LogicalDeviceAccessor device)
        : _data(std::make_shared<_impl::SynchroManager_impl>()) { _data->device = device._data; }
    SemaphoreHolder SynchroManager::createSemaphore(std::string name) { return createSemaphore(std::move(name), 1); }
    SemaphoreHolder SynchroManager::createSemaphore(std::string name, std::size_t amount) {
        if (!amount)
            throw std::invalid_argument("Semaphore count must be positive");
        auto data = std::make_shared<_impl::Semaphore_impl>();
        data->_manager = _data;
        data->device = _data->device;
        for (std::size_t i = 0; i < amount; ++i)
            data->semaphores.push_back(_data->device->device.createSemaphore({}));
        _data->semaphores[std::move(name)] = data;
        return {data};
    }
    FenceHolder SynchroManager::createFence(std::string name, bool signal) {
        return createFence(std::move(name), std::size_t{1}, signal);
    }
    FenceHolder SynchroManager::createFence(std::string name, std::size_t amount, bool signal) {
        if (!amount)
            throw std::invalid_argument("Fence count must be positive");
        auto data = std::make_shared<_impl::Fence_impl>();
        data->_manager = _data;
        data->device = _data->device;
        vk::FenceCreateInfo info{};
        if (signal)
            info.flags = vk::FenceCreateFlagBits::eSignaled;
        for (std::size_t i = 0; i < amount; ++i)
            data->fences.push_back(_data->device->device.createFence(info));
        _data->fences[std::move(name)] = data;
        return {data};
    }
    std::optional<SemaphoreHolder> SynchroManager::getSemaphore(std::string name) {
        auto found = _data->semaphores.find(name);
        if (found == _data->semaphores.end())
            return std::nullopt;
        return SemaphoreHolder{found->second};
    }
    std::optional<FenceHolder> SynchroManager::getFence(std::string name) {
        auto found = _data->fences.find(name);
        if (found == _data->fences.end())
            return std::nullopt;
        return FenceHolder{found->second};
    }
    bool SynchroManager::destroySemaphore(std::string name) {
        auto holder = getSemaphore(name);
        if (!holder)
            return false;
        holder->destroy();
        return true;
    }
    bool SynchroManager::destroyFence(std::string name) {
        auto holder = getFence(name);
        if (!holder)
            return false;
        holder->destroy();
        return true;
    }
    std::optional<FenceHolder> SynchroManager::operator[](std::string name) { return getFence(std::move(name)); }
}
