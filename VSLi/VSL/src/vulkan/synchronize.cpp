#include <VSL/lifetime/device_owner.hpp>
#include <VSL/vulkan/synchronize.hpp>
#include <VSL/vulkan/_pimpls.hpp>
#include <VSL/lifetime/synchronization_use.hpp>
#include <VSL/lifetime/synchronization_storage.hpp>
#include <VSL/lifetime/owner_access.hpp>
#include <VSL/lifetime/synchronization_access.hpp>
#include <stdexcept>

namespace VSL_NAMESPACE::lifetime::detail {
    namespace {
        template<typename T>
        void require_canonical(const std::shared_ptr<T>& owner) {
            if (!owner || !owner.use_count() || !owner->owner_node || owner->owner_node->retiring() || !owner->owner_node->matches(owner))
                throw std::invalid_argument("Synchronization resource requires its canonical owner");
        }
        template<typename T>
        void require_canonical(const OwnedRef<T>& owner) {
            require_canonical(OwnerAccess::Shared(owner));
            if (OwnerAccess::Node(owner) != owner->owner_node)
                throw std::invalid_argument("Synchronization resource requires its canonical dependency node");
        }
    } // namespace
    void SynchronizationAccess::RequireCanonical(const std::shared_ptr<impl::FenceImpl>& owner) {
        require_canonical(owner);
    }
    void SynchronizationAccess::RequireCanonical(const std::shared_ptr<impl::SemaphoreImpl>& owner) {
        require_canonical(owner);
    }
    FenceHolder SynchronizationAccess::FromOwned(OwnedRef<impl::FenceImpl> owner) {
        require_canonical(owner);
        return FenceHolder(std::move(owner));
    }
    SemaphoreHolder SynchronizationAccess::FromOwned(OwnedRef<impl::SemaphoreImpl> owner) {
        require_canonical(owner);
        return SemaphoreHolder(std::move(owner));
    }
    FenceHolder SynchronizationAccess::FromShared(std::shared_ptr<impl::FenceImpl> owner) {
        require_canonical(owner);
        const auto* node = owner->owner_node;
        return FenceHolder(node->ref(std::move(owner)));
    }
    SemaphoreHolder SynchronizationAccess::FromShared(std::shared_ptr<impl::SemaphoreImpl> owner) {
        require_canonical(owner);
        const auto* node = owner->owner_node;
        return SemaphoreHolder(node->ref(std::move(owner)));
    }
} // namespace VSL_NAMESPACE::lifetime::detail

namespace VSL_NAMESPACE {
    using lifetime::detail::OwnerAccess;
    using lifetime::detail::SynchronizationAccess;
    impl::SynchroManagerImpl::~SynchroManagerImpl() = default;
    std::size_t SemaphoreHolder::size() const {
        return _owner ? _owner->semaphores.size() : 0;
    }
    lifetime::OwnerRef SemaphoreHolder::owner_ref() const {
        lifetime::detail::require_canonical(_owner);
        return _owner;
    }
    LogicalDeviceAccessor SemaphoreHolder::get_device() const {
        lifetime::detail::require_canonical(_owner);
        if (!_owner->device || !_owner->native_device || _owner->semaphores.empty())
            throw std::invalid_argument("Semaphore has been destroyed");
        return lifetime::device_from_child(_owner);
    }
    std::size_t FenceHolder::size() const {
        return _owner ? _owner->fences.size() : 0;
    }
    lifetime::OwnerRef FenceHolder::owner_ref() const {
        lifetime::detail::require_canonical(_owner);
        return _owner;
    }
    LogicalDeviceAccessor FenceHolder::get_device() const {
        lifetime::detail::require_canonical(_owner);
        if (!_owner->device || !_owner->native_device || _owner->fences.empty())
            throw std::invalid_argument("Fence has been destroyed");
        return lifetime::device_from_child(_owner);
    }
    void SemaphoreHolder::destroy() {
        lifetime::destroy_semaphores(OwnerAccess::Shared(_owner));
    }
    bool FenceHolder::wait(std::uint64_t timeout) {
        return lifetime::wait_fences(OwnerAccess::Shared(_owner), std::nullopt, timeout);
    }
    bool FenceHolder::wait(std::uint32_t index, std::uint64_t timeout) {
        return lifetime::wait_fences(OwnerAccess::Shared(_owner), index, timeout);
    }
    void FenceHolder::reset() {
        lifetime::reset_fences(OwnerAccess::Shared(_owner), std::nullopt);
    }
    void FenceHolder::reset(std::uint32_t index) {
        lifetime::reset_fences(OwnerAccess::Shared(_owner), index);
    }
    void FenceHolder::destroy() {
        lifetime::destroy_fences(OwnerAccess::Shared(_owner));
    }
    SynchroManager::SynchroManager(LogicalDeviceAccessor device)
        : _registry(std::make_shared<impl::SynchroManagerImpl>()) {
        lifetime::require_device_owner(device);
        _registry->device = device.data_;
    }
    LogicalDeviceAccessor SynchroManager::get_device() const {
        const auto registry = _registry;
        if (!registry || !registry->device)
            throw std::invalid_argument("Synchronization manager is empty");
        return {registry->device};
    }
    SemaphoreHolder SynchroManager::create_semaphore(std::string name) {
        return create_semaphore(std::move(name), 1);
    }
    SemaphoreHolder SynchroManager::create_semaphore(std::string name, std::size_t amount) {
        const auto registry = _registry;
        if (!registry || !registry->device)
            throw std::invalid_argument("Synchronization manager is empty");
        auto data = lifetime::make_heap_semaphore({registry->device}, {amount});
        data->owner_context->registry = registry;
        auto result = SynchronizationAccess::FromShared(data);
        registry->semaphores[std::move(name)].swap(data);
        return result;
    }
    FenceHolder SynchroManager::create_fence(std::string name, bool signal) {
        return create_fence(std::move(name), std::size_t{1}, signal);
    }
    FenceHolder SynchroManager::create_fence(std::string name, std::size_t amount, bool signal) {
        const auto registry = _registry;
        if (!registry || !registry->device)
            throw std::invalid_argument("Synchronization manager is empty");
        auto data = lifetime::make_heap_fence({registry->device}, {amount, signal});
        data->owner_context->registry = registry;
        auto result = SynchronizationAccess::FromShared(data);
        registry->fences[std::move(name)].swap(data);
        return result;
    }
    std::optional<SemaphoreHolder> SynchroManager::get_semaphore(std::string name) {
        const auto registry = _registry;
        if (!registry)
            return std::nullopt;
        const auto found = registry->semaphores.find(name);
        return found != registry->semaphores.end() ? std::optional(SynchronizationAccess::FromShared(found->second)) : std::nullopt;
    }
    std::optional<FenceHolder> SynchroManager::get_fence(std::string name) {
        const auto registry = _registry;
        if (!registry)
            return std::nullopt;
        const auto found = registry->fences.find(name);
        return found != registry->fences.end() ? std::optional(SynchronizationAccess::FromShared(found->second)) : std::nullopt;
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
