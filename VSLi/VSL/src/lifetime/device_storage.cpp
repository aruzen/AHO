#include <VSL/lifetime/device_storage.hpp>
#include <VSL/lifetime/device_owner.hpp>
#include <VSL/lifetime/owner_access.hpp>
#include <new>

namespace VSL_NAMESPACE::lifetime {
    static_assert(std::is_standard_layout_v<DeviceSlotStorage> && offsetof(DeviceSlotStorage, value) == 0);
    static_assert(std::is_nothrow_default_constructible_v<impl::LogicalDeviceImpl>);
    static_assert(std::is_nothrow_default_constructible_v<DeviceOwnerContext>);
    namespace {
        struct HeapDeviceOwner {
            DependencyNode node;
            OwnerRef physical;
            alignas(DeviceSlotStorage) std::byte storage[sizeof(DeviceSlotStorage)];
            HeapDeviceOwner(PhysicalDevice physical, DeviceCreateInfo info);
            ~HeapDeviceOwner();
            static void VisitDependencies(const void* owner, DependencyNode::EdgeVisitor visitor, void* context);
        };
        HeapDeviceOwner::HeapDeviceOwner(PhysicalDevice physical, DeviceCreateInfo info) : physical(physical.owner_ref()) {
            construct_device(storage, *physical.data_, std::move(info));
            try {
                set_device_physical_owner(device_at(storage), this->physical.alias(physical.data_.get()));
            } catch (...) {
                destroy_device(storage);
                throw;
            }
        }
        HeapDeviceOwner::~HeapDeviceOwner() {
            node.retire();
            destroy_device(storage);
        }
        void HeapDeviceOwner::VisitDependencies(const void* owner, DependencyNode::EdgeVisitor visitor, void* context) {
            visitor(context, static_cast<const HeapDeviceOwner*>(owner)->physical);
        }
    } // namespace
    void construct_device(void* storage, impl::PhysicalDeviceImpl& physical, DeviceCreateInfo info, const operation::DeviceNativeCalls& calls) {
        auto* slot = std::construct_at(static_cast<DeviceSlotStorage*>(storage));
        auto* context = std::construct_at(reinterpret_cast<DeviceOwnerContext*>(slot->context));
        auto* device = std::construct_at(reinterpret_cast<impl::LogicalDeviceImpl*>(slot->value));
        device->owner_context = context;
        try {
            operation::initialize_device(*device, physical, std::move(info), calls);
        } catch (...) {
            std::destroy_at(device);
            std::destroy_at(context);
            std::destroy_at(slot);
            throw;
        }
    }
    impl::LogicalDeviceImpl& device_at(void* storage) {
        return *std::launder(reinterpret_cast<impl::LogicalDeviceImpl*>(static_cast<DeviceSlotStorage*>(storage)->value));
    }
    void destroy_device(void* storage) noexcept {
        auto* context = device_at(storage).owner_context;
        std::destroy_at(&device_at(storage));
        std::destroy_at(context);
        std::destroy_at(static_cast<DeviceSlotStorage*>(storage));
    }
    void set_device_physical_owner(impl::LogicalDeviceImpl& target, OwnedRef<impl::PhysicalDeviceImpl> physical) {
        if (!target.owner_context || !physical.use_count() || physical.get() != target.parent_device || !physical->owner_node ||
            physical->owner_node->retiring() || !physical->owner_node->matches(detail::OwnerAccess::Shared(physical)))
            throw std::invalid_argument("Device getter requires its canonical PhysicalDevice parent");
        target.owner_context->physical = physical.weak();
    }
    void require_device_owner(const LogicalDeviceAccessor& device) {
        const auto& source = device.data_;
        if (!source || !source.use_count() || !source->device || !source->owner_node || source->owner_node->retiring() || !source->owner_node->matches(source))
            throw std::invalid_argument("Device requires its live canonical owner");
    }
    LogicalDeviceAccessor device_from_impl(impl::LogicalDeviceImpl& device) {
        if (!device.device || !device.owner_node || device.owner_node->retiring())
            throw std::invalid_argument("Device getter requires a live canonical Device");
        return {device.owner_node->owner_ref().alias(&device).share()};
    }
    std::shared_ptr<impl::LogicalDeviceImpl> make_heap_device(PhysicalDevice physical, DeviceCreateInfo info) {
        auto owner = std::make_shared<HeapDeviceOwner>(std::move(physical), std::move(info));
        auto* device = &device_at(owner->storage);
        owner->node.publish(owner, owner.get(), HeapDeviceOwner::VisitDependencies);
        device->owner_node = &owner->node;
        return {std::move(owner), device};
    }
} // namespace VSL_NAMESPACE::lifetime
