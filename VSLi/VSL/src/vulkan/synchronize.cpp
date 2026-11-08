#include <VSL/vulkan/synchronize.hpp>
#include <VSL/vulkan/_pimpls.hpp>
#include <VSL/lifetime/synchronization_use.hpp>
#include <VSL/operation/synchronization_ops.hpp>
#include <stdexcept>

namespace VSL_NAMESPACE {
    impl::SemaphoreImpl::~SemaphoreImpl() {
        if (!semaphores.empty())
            operation::destroy_semaphores(device->device, semaphores);
    }
    impl::FenceImpl::~FenceImpl() {
        if (!fences.empty())
            operation::destroy_fences(device->device, fences);
    }
    impl::SynchroManagerImpl::~SynchroManagerImpl() = default;
    std::size_t SemaphoreHolder::size() const {
        return data_ ? data_->semaphores.size() : 0;
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
        if (!device.data_ || !device.data_.use_count() || !device.data_->device)
            throw std::invalid_argument("Synchronization requires a logical device owner");
        data_->device = device.data_;
    }
    SemaphoreHolder SynchroManager::create_semaphore(std::string name) {
        return create_semaphore(std::move(name), 1);
    }
    SemaphoreHolder SynchroManager::create_semaphore(std::string name, std::size_t amount) {
        const auto registry = data_;
        if (!registry || !registry->device)
            throw std::invalid_argument("Synchronization manager is empty");
        auto data = std::make_shared<impl::SemaphoreImpl>();
        data->manager = registry;
        data->device = registry->device;
        data->semaphores = operation::create_semaphores(data->device->device, amount);
        data->owner = data;
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
        operation::validate_synchronization_count(registry->device->device, amount);
        auto data = std::make_shared<impl::FenceImpl>();
        data->manager = registry;
        data->device = registry->device;
        data->uses.resize(amount);
        data->fences = operation::create_fences(data->device->device, amount, signal);
        data->owner = data;
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
