#include <VSL/lifetime/device_owner.hpp>
#include <VSL/lifetime/synchronization_storage.hpp>
#include <VSL/lifetime/owner_access.hpp>
#include <new>
#include <stdexcept>

namespace VSL_NAMESPACE::lifetime {
    static_assert(std::is_standard_layout_v<SemaphoreSlotStorage> && offsetof(SemaphoreSlotStorage, value) == 0);
    static_assert(std::is_standard_layout_v<FenceSlotStorage> && offsetof(FenceSlotStorage, value) == 0);
    static_assert(std::is_nothrow_default_constructible_v<impl::SemaphoreImpl>);
    static_assert(std::is_nothrow_default_constructible_v<impl::FenceImpl>);
    static_assert(std::is_nothrow_default_constructible_v<SynchronizationOwnerContext>);
    namespace {
        template<typename Slot, typename Info, void (*Construct)(void*, impl::LogicalDeviceImpl&, Info), void (*Destroy)(void*) noexcept>
        struct HeapSynchronizationOwner {
            DependencyNode node;
            LogicalDeviceAccessor device;
            alignas(Slot) std::byte storage[sizeof(Slot)];
            HeapSynchronizationOwner(LogicalDeviceAccessor device, Info info);
            ~HeapSynchronizationOwner();
            static void VisitDependencies(const void* owner, DependencyNode::EdgeVisitor visitor, void* context);
        };
        template<typename Slot, typename Info, void (*Construct)(void*, impl::LogicalDeviceImpl&, Info), void (*Destroy)(void*) noexcept>
        HeapSynchronizationOwner<Slot, Info, Construct, Destroy>::HeapSynchronizationOwner(LogicalDeviceAccessor device, Info info) : device(device) {
            require_device_owner(device);
            Construct(storage, *device.data_, info);
        }
        template<typename Slot, typename Info, void (*Construct)(void*, impl::LogicalDeviceImpl&, Info), void (*Destroy)(void*) noexcept>
        HeapSynchronizationOwner<Slot, Info, Construct, Destroy>::~HeapSynchronizationOwner() {
            node.retire();
            Destroy(storage);
        }
        template<typename Slot, typename Info, void (*Construct)(void*, impl::LogicalDeviceImpl&, Info), void (*Destroy)(void*) noexcept>
        void HeapSynchronizationOwner<Slot, Info, Construct, Destroy>::VisitDependencies(const void* owner, DependencyNode::EdgeVisitor visitor, void* context) {
            visitor(context, static_cast<const HeapSynchronizationOwner*>(owner)->device.owner_ref());
        }
        template<typename Slot, typename Impl, typename Info, void (*Initialize)(Impl&, impl::LogicalDeviceImpl&, Info)>
        void construct(void* storage, impl::LogicalDeviceImpl& device, Info info) {
            if (!device.device)
                throw std::invalid_argument("Synchronization requires a logical device owner");
            auto* slot = std::construct_at(static_cast<Slot*>(storage));
            auto* context = std::construct_at(reinterpret_cast<SynchronizationOwnerContext*>(slot->context));
            auto* value = std::construct_at(reinterpret_cast<Impl*>(slot->value));
            value->owner_context = context;
            try {
                Initialize(*value, device, info);
            } catch (...) {
                std::destroy_at(value);
                std::destroy_at(context);
                std::destroy_at(slot);
                throw;
            }
        }
    } // namespace
    void construct_semaphore(void* storage, LogicalDeviceAccessor device, SemaphoreCreateInfo info) {
        require_device_owner(device);
        construct_semaphore(storage, *device.data_, info);
    }
    void construct_semaphore(void* storage, impl::LogicalDeviceImpl& device, SemaphoreCreateInfo info) {
        construct<SemaphoreSlotStorage, impl::SemaphoreImpl, SemaphoreCreateInfo, operation::initialize_semaphore>(storage, device, info);
    }
    void construct_fence(void* storage, LogicalDeviceAccessor device, FenceCreateInfo info) {
        require_device_owner(device);
        construct_fence(storage, *device.data_, info);
    }
    void construct_fence(void* storage, impl::LogicalDeviceImpl& device, FenceCreateInfo info) {
        construct<FenceSlotStorage, impl::FenceImpl, FenceCreateInfo, operation::initialize_fence>(storage, device, info);
    }
    impl::SemaphoreImpl& semaphore_at(void* storage) {
        return *std::launder(reinterpret_cast<impl::SemaphoreImpl*>(static_cast<SemaphoreSlotStorage*>(storage)->value));
    }
    impl::FenceImpl& fence_at(void* storage) {
        return *std::launder(reinterpret_cast<impl::FenceImpl*>(static_cast<FenceSlotStorage*>(storage)->value));
    }
    void destroy_semaphore(void* storage) noexcept {
        auto* context = semaphore_at(storage).owner_context;
        std::destroy_at(&semaphore_at(storage));
        std::destroy_at(context);
        std::destroy_at(static_cast<SemaphoreSlotStorage*>(storage));
    }
    void destroy_fence(void* storage) noexcept {
        auto* context = fence_at(storage).owner_context;
        std::destroy_at(&fence_at(storage));
        std::destroy_at(context);
        std::destroy_at(static_cast<FenceSlotStorage*>(storage));
    }
    std::shared_ptr<impl::SemaphoreImpl> make_heap_semaphore(LogicalDeviceAccessor device, SemaphoreCreateInfo info) {
        using Owner = HeapSynchronizationOwner<SemaphoreSlotStorage, SemaphoreCreateInfo, construct_semaphore, destroy_semaphore>;
        auto owner = std::make_shared<Owner>(device, info);
        auto* value = &semaphore_at(owner->storage);
        owner->node.publish(owner, owner.get(), Owner::VisitDependencies);
        value->owner_node = &owner->node;
        return {std::move(owner), value};
    }
    std::shared_ptr<impl::FenceImpl> make_heap_fence(LogicalDeviceAccessor device, FenceCreateInfo info) {
        using Owner = HeapSynchronizationOwner<FenceSlotStorage, FenceCreateInfo, construct_fence, destroy_fence>;
        auto owner = std::make_shared<Owner>(device, info);
        auto* value = &fence_at(owner->storage);
        owner->node.publish(owner, owner.get(), Owner::VisitDependencies);
        value->owner_node = &owner->node;
        return {std::move(owner), value};
    }
} // namespace VSL_NAMESPACE::lifetime

VSL_NAMESPACE::impl::SemaphoreImpl::~SemaphoreImpl() {
    operation::destroy_semaphores(native_device, semaphores);
}
VSL_NAMESPACE::impl::FenceImpl::~FenceImpl() {
    operation::destroy_fences(native_device, fences);
}
