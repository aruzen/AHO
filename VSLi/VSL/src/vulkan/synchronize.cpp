#include <VSL/vulkan/synchronize.hpp>
#include <VSL/vulkan/_pimpls.hpp>
#include <stdexcept>

namespace VSL_NAMESPACE {
    namespace {
        template<class Map, class T>
        void erase_handle(Map& map, const std::shared_ptr<T>& target) {
            std::erase_if(map, [&](const auto& entry) { return entry.second == target; });
        }
        void require_fence(const std::shared_ptr<impl::FenceImpl>& data) {
            if (!data || data->fences.empty())
                throw std::logic_error("Fence has been destroyed");
        }
        void remember_completion(const std::shared_ptr<impl::FenceImpl>& data, std::size_t index) {
            if (index < data->submissions.size())
                if (auto submission = data->submissions[index].lock())
                    (void)Completion{submission}.status();
        }
        void remember_completions(const std::shared_ptr<impl::FenceImpl>& data) {
            // 公開APIでフェンスをリセット・破棄する前に、以前の送信結果を保存する。
            for (std::size_t i = 0; i < data->submissions.size(); ++i)
                remember_completion(data, i);
        }
    } // namespace
    impl::SemaphoreImpl::~SemaphoreImpl() {
        for (auto handle : semaphores)
            device->device.destroySemaphore(handle);
    }
    impl::FenceImpl::~FenceImpl() {
        for (auto handle : fences)
            device->device.destroyFence(handle);
    }
    impl::SynchroManagerImpl::~SynchroManagerImpl() = default;

    std::size_t SemaphoreHolder::size() const {
        return data_ ? data_->semaphores.size() : 0;
    }
    void SemaphoreHolder::destroy() {
        if (!data_)
            return;
        auto data = data_; // マップから削除しても参照が無効にならないよう、共有ポインタをコピーする。
        if (auto manager = data->manager.lock())
            erase_handle(manager->semaphores, data);
        for (auto handle : data->semaphores)
            data->device->device.destroySemaphore(handle);
        data->semaphores.clear();
    }
    bool FenceHolder::wait(std::uint64_t timeout) {
        require_fence(data_);
        const bool ready = data_->device->device.waitForFences(data_->fences, true, timeout) == vk::Result::eSuccess;
        if (ready)
            remember_completions(data_);
        return ready;
    }
    bool FenceHolder::wait(std::uint32_t index, std::uint64_t timeout) {
        require_fence(data_);
        const bool ready = data_->device->device.waitForFences(data_->fences.at(index), true, timeout) == vk::Result::eSuccess;
        if (ready)
            remember_completion(data_, index);
        return ready;
    }
    void FenceHolder::reset() {
        require_fence(data_);
        remember_completions(data_);
        data_->device->device.resetFences(data_->fences);
    }
    void FenceHolder::reset(std::uint32_t index) {
        require_fence(data_);
        remember_completion(data_, index);
        data_->device->device.resetFences(data_->fences.at(index));
    }
    void FenceHolder::destroy() {
        if (!data_)
            return;
        auto data = data_;
        remember_completions(data);
        if (auto manager = data->manager.lock())
            erase_handle(manager->fences, data);
        for (auto handle : data->fences)
            data->device->device.destroyFence(handle);
        data->fences.clear();
        data->submissions.clear();
    }
    SynchroManager::SynchroManager(LogicalDeviceAccessor device)
        : data_(std::make_shared<impl::SynchroManagerImpl>()) {
        data_->device = device.data_;
    }
    SemaphoreHolder SynchroManager::create_semaphore(std::string name) {
        return create_semaphore(std::move(name), 1);
    }
    SemaphoreHolder SynchroManager::create_semaphore(std::string name, std::size_t amount) {
        if (!amount)
            throw std::invalid_argument("Semaphore count must be positive");
        auto data = std::make_shared<impl::SemaphoreImpl>();
        data->manager = data_;
        data->device = data_->device;
        for (std::size_t i = 0; i < amount; ++i)
            data->semaphores.push_back(data_->device->device.createSemaphore({}));
        data_->semaphores[std::move(name)] = data;
        return {data};
    }
    FenceHolder SynchroManager::create_fence(std::string name, bool signal) {
        return create_fence(std::move(name), std::size_t{1}, signal);
    }
    FenceHolder SynchroManager::create_fence(std::string name, std::size_t amount, bool signal) {
        if (!amount)
            throw std::invalid_argument("Fence count must be positive");
        auto data = std::make_shared<impl::FenceImpl>();
        data->manager = data_;
        data->device = data_->device;
        vk::FenceCreateInfo info{};
        if (signal)
            info.flags = vk::FenceCreateFlagBits::eSignaled;
        for (std::size_t i = 0; i < amount; ++i)
            data->fences.push_back(data_->device->device.createFence(info));
        data_->fences[std::move(name)] = data;
        return {data};
    }
    std::optional<SemaphoreHolder> SynchroManager::get_semaphore(std::string name) {
        auto found = data_->semaphores.find(name);
        if (found == data_->semaphores.end())
            return std::nullopt;
        return SemaphoreHolder{found->second};
    }
    std::optional<FenceHolder> SynchroManager::get_fence(std::string name) {
        auto found = data_->fences.find(name);
        if (found == data_->fences.end())
            return std::nullopt;
        return FenceHolder{found->second};
    }
    bool SynchroManager::destroy_semaphore(std::string name) {
        auto holder = get_semaphore(name);
        if (!holder)
            return false;
        holder->destroy();
        return true;
    }
    bool SynchroManager::destroy_fence(std::string name) {
        auto holder = get_fence(name);
        if (!holder)
            return false;
        holder->destroy();
        return true;
    }
    std::optional<FenceHolder> SynchroManager::operator[](std::string name) {
        return get_fence(std::move(name));
    }
} // namespace VSL_NAMESPACE
