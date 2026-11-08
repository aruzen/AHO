#include <VSL/lifetime/instance_storage.hpp>
#include <VSL/lifetime/owner_access.hpp>
#include <new>

namespace VSL_NAMESPACE::lifetime {
    static_assert(std::is_standard_layout_v<InstanceSlotStorage> && offsetof(InstanceSlotStorage, value) == 0);
    static_assert(std::is_nothrow_default_constructible_v<impl::VulkanImplAccessor>);
    namespace {
        struct HeapInstanceOwner {
            DependencyNode node;
            alignas(InstanceSlotStorage) std::byte storage[sizeof(InstanceSlotStorage)];
            explicit HeapInstanceOwner(InstanceCreateInfo info);
            ~HeapInstanceOwner();
        };
        HeapInstanceOwner::HeapInstanceOwner(InstanceCreateInfo info) {
            construct_instance(storage, std::move(info));
        }
        HeapInstanceOwner::~HeapInstanceOwner() {
            node.retire();
            destroy_instance(storage);
        }
    } // namespace
    void construct_instance(void* storage, InstanceCreateInfo info, const operation::InstanceNativeCalls& calls) {
        auto* slot = std::construct_at(static_cast<InstanceSlotStorage*>(storage));
        auto* instance = std::construct_at(reinterpret_cast<impl::VulkanImplAccessor*>(slot->value));
        try {
            operation::initialize_instance(*instance, std::move(info), calls);
        } catch (...) {
            std::destroy_at(instance);
            std::destroy_at(slot);
            throw;
        }
    }
    impl::VulkanImplAccessor& instance_at(void* storage) {
        return *std::launder(reinterpret_cast<impl::VulkanImplAccessor*>(static_cast<InstanceSlotStorage*>(storage)->value));
    }
    void destroy_instance(void* storage) noexcept {
        std::destroy_at(&instance_at(storage));
        std::destroy_at(static_cast<InstanceSlotStorage*>(storage));
    }
    std::shared_ptr<impl::VulkanImplAccessor> make_heap_instance(InstanceCreateInfo info) {
        auto owner = std::make_shared<HeapInstanceOwner>(std::move(info));
        auto* instance = &instance_at(owner->storage);
        return detail::publish_leaf(std::move(owner), instance);
    }
} // namespace VSL_NAMESPACE::lifetime

VSL_NAMESPACE::impl::VulkanImplAccessor::~VulkanImplAccessor() {
    operation::destroy_instance(*this);
}
