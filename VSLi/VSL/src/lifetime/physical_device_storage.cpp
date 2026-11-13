#include <VSL/lifetime/startup_access.hpp>
#include <VSL/lifetime/physical_device_storage.hpp>
#include <VSL/lifetime/owner_access.hpp>
#include <new>
#include <stdexcept>

namespace {
    using VSL_NAMESPACE::lifetime::detail::StartupAccess;
}

namespace VSL_NAMESPACE::lifetime {
    static_assert(std::is_standard_layout_v<PhysicalDeviceSlotStorage> && offsetof(PhysicalDeviceSlotStorage, value) == 0);
    static_assert(std::is_nothrow_default_constructible_v<impl::PhysicalDeviceImpl>);
    static_assert(std::is_nothrow_default_constructible_v<PhysicalDeviceOwnerContext>);
    namespace {
        struct HeapPhysicalDeviceOwner {
            DependencyNode node;
            OwnerRef instance;
            alignas(PhysicalDeviceSlotStorage) std::byte storage[sizeof(PhysicalDeviceSlotStorage)];
            HeapPhysicalDeviceOwner(VulkanAccessor instance, const operation::PhysicalDeviceCandidate& candidate);
            ~HeapPhysicalDeviceOwner();
            static void VisitDependencies(const void* owner, DependencyNode::EdgeVisitor visitor, void* context);
        };
        HeapPhysicalDeviceOwner::HeapPhysicalDeviceOwner(VulkanAccessor instance, const operation::PhysicalDeviceCandidate& candidate) : instance(instance.owner_ref()) {
            construct_physical_device(storage, *StartupAccess::Owned(instance), candidate);
            try {
                set_physical_device_instance_owner(physical_device_at(storage), this->instance.alias(StartupAccess::Owned(instance).get()));
            } catch (...) {
                destroy_physical_device(storage);
                throw;
            }
        }
        HeapPhysicalDeviceOwner::~HeapPhysicalDeviceOwner() {
            node.retire();
            destroy_physical_device(storage);
        }
        void HeapPhysicalDeviceOwner::VisitDependencies(const void* owner, DependencyNode::EdgeVisitor visitor, void* context) {
            visitor(context, static_cast<const HeapPhysicalDeviceOwner*>(owner)->instance);
        }
    } // namespace
    void construct_physical_device(void* storage, impl::VulkanImplAccessor& instance, const operation::PhysicalDeviceCandidate& candidate) {
        auto* slot = std::construct_at(static_cast<PhysicalDeviceSlotStorage*>(storage));
        auto* context = std::construct_at(reinterpret_cast<PhysicalDeviceOwnerContext*>(slot->context));
        auto* device = std::construct_at(reinterpret_cast<impl::PhysicalDeviceImpl*>(slot->value));
        device->owner_context = context;
        try {
            operation::initialize_physical_device(*device, instance, candidate);
        } catch (...) {
            std::destroy_at(device);
            std::destroy_at(context);
            std::destroy_at(slot);
            throw;
        }
    }
    impl::PhysicalDeviceImpl& physical_device_at(void* storage) {
        return *std::launder(reinterpret_cast<impl::PhysicalDeviceImpl*>(static_cast<PhysicalDeviceSlotStorage*>(storage)->value));
    }
    void destroy_physical_device(void* storage) noexcept {
        auto* context = physical_device_at(storage).owner_context;
        std::destroy_at(&physical_device_at(storage));
        std::destroy_at(context);
        std::destroy_at(static_cast<PhysicalDeviceSlotStorage*>(storage));
    }
    void set_physical_device_instance_owner(impl::PhysicalDeviceImpl& target, OwnedRef<impl::VulkanImplAccessor> instance) {
        if (!target.owner_context || !target.vulkan || !instance.use_count() || instance.get() != target.vulkan || !target.vulkan->owner_node || target.vulkan->owner_node->retiring() ||
            !target.vulkan->owner_node->matches(detail::OwnerAccess::Shared(instance)))
            throw std::invalid_argument("Physical device getter requires its canonical Instance owner");
        target.owner_context->instance = instance.weak();
    }
    std::shared_ptr<impl::PhysicalDeviceImpl> make_heap_physical_device(VulkanAccessor instance, const operation::PhysicalDeviceCandidate& candidate) {
        auto owner = std::make_shared<HeapPhysicalDeviceOwner>(std::move(instance), candidate);
        auto* device = &physical_device_at(owner->storage);
        owner->node.publish(owner, owner.get(), HeapPhysicalDeviceOwner::VisitDependencies);
        device->owner_node = &owner->node;
        return {std::move(owner), device};
    }
} // namespace VSL_NAMESPACE::lifetime
