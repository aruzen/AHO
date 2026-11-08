#include <VSL/lifetime/device_owner.hpp>
#include <VSL/vulkan/synchronize.hpp>
#include <VSL/vulkan/_pimpls.hpp>
#include <VSL/lifetime/synchronization_use.hpp>
#include <VSL/lifetime/synchronization_storage.hpp>
#include <VSL/lifetime/owner_access.hpp>
#include <stdexcept>

namespace VSL_NAMESPACE {
    impl::SynchroManagerImpl::~SynchroManagerImpl() = default;
    std::size_t SemaphoreHolder::size() const {
        return data_ ? data_->semaphores.size() : 0;
    }
    lifetime::OwnerRef SemaphoreHolder::owner_ref() const {
        if (!data_ || !data_.use_count() || !data_->owner_node || !data_->owner_node->matches(data_))
            throw std::invalid_argument("Semaphore requires its canonical owner");
        return data_->owner_node->ref(data_);
    }
    LogicalDeviceAccessor SemaphoreHolder::get_device() const {
        (void)owner_ref();
        if (!data_->device || !data_->native_device || data_->semaphores.empty())
            throw std::invalid_argument("Semaphore has been destroyed");
        return lifetime::device_from_child(data_);
    }
    std::size_t FenceHolder::size() const {
        return data_ ? data_->fences.size() : 0;
    }
    lifetime::OwnerRef FenceHolder::owner_ref() const {
        if (!data_ || !data_.use_count() || !data_->owner_node || !data_->owner_node->matches(data_))
            throw std::invalid_argument("Fence requires its canonical owner");
        return data_->owner_node->ref(data_);
    }
    LogicalDeviceAccessor FenceHolder::get_device() const {
        (void)owner_ref();
        if (!data_->device || !data_->native_device || data_->fences.empty())
            throw std::invalid_argument("Fence has been destroyed");
        return lifetime::device_from_child(data_);
    }
    void SemaphoreHolder::destroy() {
        lifetime::destroy_semaphores(data_);
    }
    bool FenceHolder::wait(std::uint64_t timeout) {
        return lifetime::wait_fences(data_, std::nullopt, timeout);
    }
    bool FenceHolder::wait(std::uint32_t index, std::uint64_t timeout) {
        return lifetime::wait_fences(data_, index, timeout);
    }
    void FenceHolder::reset() {
        lifetime::reset_fences(data_, std::nullopt);
    }
    void FenceHolder::reset(std::uint32_t index) {
        lifetime::reset_fences(data_, index);
    }
    void FenceHolder::destroy() {
        lifetime::destroy_fences(data_);
    }
    SynchroManager::SynchroManager(LogicalDeviceAccessor device)
        : data_(std::make_shared<impl::SynchroManagerImpl>()) {
        lifetime::require_device_owner(device);
        data_->device = device.data_;
    }
    SemaphoreHolder SynchroManager::create_semaphore(std::string name) {
        return create_semaphore(std::move(name), 1);
    }
    SemaphoreHolder SynchroManager::create_semaphore(std::string name, std::size_t amount) {
        const auto registry = data_;
        if (!registry || !registry->device)
            throw std::invalid_argument("Synchronization manager is empty");
        auto data = lifetime::make_heap_semaphore({registry->device}, {amount});
        data->owner_context->registry = registry;
        registry->semaphores[std::move(name)] = data;
        return {std::move(data)};
    }
    FenceHolder SynchroManager::create_fence(std::string name, bool signal) {
        return create_fence(std::move(name), std::size_t{1}, signal);
    }
    FenceHolder SynchroManager::create_fence(std::string name, std::size_t amount, bool signal) {
        const auto registry = data_;
        if (!registry || !registry->device)
            throw std::invalid_argument("Synchronization manager is empty");
        auto data = lifetime::make_heap_fence({registry->device}, {amount, signal});
        data->owner_context->registry = registry;
        registry->fences[std::move(name)] = data;
        return {std::move(data)};
    }
    std::optional<SemaphoreHolder> SynchroManager::get_semaphore(std::string name) {
        const auto registry = data_;
        if (!registry)
            return std::nullopt;
        const auto found = registry->semaphores.find(name);
        return found != registry->semaphores.end() ? std::optional(SemaphoreHolder{found->second}) : std::nullopt;
    }
    std::optional<FenceHolder> SynchroManager::get_fence(std::string name) {
        const auto registry = data_;
        if (!registry)
            return std::nullopt;
        const auto found = registry->fences.find(name);
        return found != registry->fences.end() ? std::optional(FenceHolder{found->second}) : std::nullopt;
    }
    bool SynchroManager::destroy_semaphore(std::string name) {
        auto holder = get_semaphore(std::move(name));
        if (!holder)
            return false;
        holder->destroy();
        return true;
    }
    bool SynchroManager::destroy_fence(std::string name) {
        auto holder = get_fence(std::move(name));
        if (!holder)
            return false;
        holder->destroy();
        return true;
    }
    std::optional<FenceHolder> SynchroManager::operator[](std::string name) {
        return get_fence(std::move(name));
    }
} // namespace VSL_NAMESPACE
