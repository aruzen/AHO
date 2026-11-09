#include <VSL/lifetime/command_access.hpp>
#include <VSL/lifetime/device_owner.hpp>
#include <VSL/lifetime/command_storage.hpp>
#include <VSL/lifetime/owner_access.hpp>
#include <new>
#include <stdexcept>

namespace {
    using VSL_NAMESPACE::lifetime::CommandAccess;
}
namespace VSL_NAMESPACE::lifetime {
    static_assert(std::is_standard_layout_v<CommandPoolSlotStorage> && offsetof(CommandPoolSlotStorage, value) == 0);
    static_assert(std::is_standard_layout_v<CommandBufferSlotStorage> && offsetof(CommandBufferSlotStorage, value) == 0);
    static_assert(std::is_nothrow_default_constructible_v<impl::CommandPoolImpl>);
    static_assert(std::is_nothrow_default_constructible_v<impl::CommandBufferImpl>);
    static_assert(std::is_nothrow_default_constructible_v<CommandBufferOwnerContext>);
    namespace {
        struct HeapCommandPoolOwner {
            DependencyNode node;
            LogicalDeviceAccessor device;
            alignas(CommandPoolSlotStorage) std::byte storage[sizeof(CommandPoolSlotStorage)];
            HeapCommandPoolOwner(LogicalDeviceAccessor device, CommandPoolCreateInfo info);
            ~HeapCommandPoolOwner();
            static void VisitDependencies(const void* owner, DependencyNode::EdgeVisitor visitor, void* context);
        };
        struct HeapCommandBufferOwner {
            DependencyNode node;
            OwnerRef pool;
            alignas(CommandBufferSlotStorage) std::byte storage[sizeof(CommandBufferSlotStorage)];
            HeapCommandBufferOwner(CommandPool pool, std::uint32_t count);
            ~HeapCommandBufferOwner();
            static void VisitDependencies(const void* owner, DependencyNode::EdgeVisitor visitor, void* context);
        };
        HeapCommandPoolOwner::HeapCommandPoolOwner(LogicalDeviceAccessor device, CommandPoolCreateInfo info) : device(device) {
            require_device_owner(device);
            construct_command_pool(storage, *device.data_, info);
        }
        HeapCommandPoolOwner::~HeapCommandPoolOwner() {
            node.retire();
            destroy_command_pool(storage);
        }
        void HeapCommandPoolOwner::VisitDependencies(const void* owner, DependencyNode::EdgeVisitor visitor, void* context) {
            visitor(context, static_cast<const HeapCommandPoolOwner*>(owner)->device.owner_ref());
        }
        HeapCommandBufferOwner::HeapCommandBufferOwner(CommandPool pool, std::uint32_t count) : pool(pool.owner_ref()) {
            construct_command_buffer(storage, *CommandAccess::Data(pool), count);
            try {
                set_command_buffer_pool_owner(command_buffer_at(storage), this->pool.alias(CommandAccess::Data(pool).get()));
            } catch (...) {
                destroy_command_buffer(storage);
                throw;
            }
        }
        HeapCommandBufferOwner::~HeapCommandBufferOwner() {
            node.retire();
            destroy_command_buffer(storage);
        }
        void HeapCommandBufferOwner::VisitDependencies(const void* owner, DependencyNode::EdgeVisitor visitor, void* context) {
            visitor(context, static_cast<const HeapCommandBufferOwner*>(owner)->pool);
        }
    } // namespace
    void construct_command_pool(void* storage, LogicalDeviceAccessor device, CommandPoolCreateInfo info) {
        require_device_owner(device);
        construct_command_pool(storage, *device.data_, info);
    }
    void construct_command_pool(void* storage, impl::LogicalDeviceImpl& device, CommandPoolCreateInfo info) {
        if (!device.device)
            throw std::invalid_argument("Command pool requires a logical device owner");
        auto* slot = std::construct_at(static_cast<CommandPoolSlotStorage*>(storage));
        auto* pool = std::construct_at(reinterpret_cast<impl::CommandPoolImpl*>(slot->value));
        try {
            operation::initialize_command_pool(*pool, device, info);
        } catch (...) {
            std::destroy_at(pool);
            std::destroy_at(slot);
            throw;
        }
    }
    void construct_command_buffer(void* storage, impl::CommandPoolImpl& pool, std::uint32_t count) {
        auto* slot = std::construct_at(static_cast<CommandBufferSlotStorage*>(storage));
        auto* context = std::construct_at(reinterpret_cast<CommandBufferOwnerContext*>(slot->context));
        auto* buffer = std::construct_at(reinterpret_cast<impl::CommandBufferImpl*>(slot->value));
        buffer->owner_context = context;
        try {
            operation::initialize_command_buffer(*buffer, pool, count);
        } catch (...) {
            std::destroy_at(buffer);
            std::destroy_at(context);
            std::destroy_at(slot);
            throw;
        }
    }
    impl::CommandPoolImpl& command_pool_at(void* storage) {
        return *std::launder(reinterpret_cast<impl::CommandPoolImpl*>(static_cast<CommandPoolSlotStorage*>(storage)->value));
    }
    impl::CommandBufferImpl& command_buffer_at(void* storage) {
        return *std::launder(reinterpret_cast<impl::CommandBufferImpl*>(static_cast<CommandBufferSlotStorage*>(storage)->value));
    }
    void destroy_command_pool(void* storage) noexcept {
        std::destroy_at(&command_pool_at(storage));
        std::destroy_at(static_cast<CommandPoolSlotStorage*>(storage));
    }
    void destroy_command_buffer(void* storage) noexcept {
        auto* context = command_buffer_at(storage).owner_context;
        std::destroy_at(&command_buffer_at(storage));
        std::destroy_at(context);
        std::destroy_at(static_cast<CommandBufferSlotStorage*>(storage));
    }
    void set_command_buffer_pool_owner(impl::CommandBufferImpl& buffer, OwnedRef<impl::CommandPoolImpl> pool) {
        if (!buffer.owner_context || !pool.use_count() || pool.get() != buffer.command_pool)
            throw std::invalid_argument("Command buffer getter requires its canonical pool owner");
        buffer.owner_context->pool = pool.weak();
    }
    std::shared_ptr<impl::CommandPoolImpl> make_heap_command_pool(LogicalDeviceAccessor device, CommandPoolCreateInfo info) {
        auto owner = std::make_shared<HeapCommandPoolOwner>(device, info);
        auto* pool = &command_pool_at(owner->storage);
        owner->node.publish(owner, owner.get(), HeapCommandPoolOwner::VisitDependencies);
        pool->owner_node = &owner->node;
        return {std::move(owner), pool};
    }
    std::shared_ptr<impl::CommandBufferImpl> make_heap_command_buffer(CommandPool pool, std::uint32_t count) {
        auto owner = std::make_shared<HeapCommandBufferOwner>(pool, count);
        auto* buffer = &command_buffer_at(owner->storage);
        owner->node.publish(owner, owner.get(), HeapCommandBufferOwner::VisitDependencies);
        buffer->owner_node = &owner->node;
        return {std::move(owner), buffer};
    }
} // namespace VSL_NAMESPACE::lifetime

VSL_NAMESPACE::impl::CommandPoolImpl::~CommandPoolImpl() {
    operation::destroy_command_pool(*this);
}
VSL_NAMESPACE::impl::CommandBufferImpl::~CommandBufferImpl() {
    operation::destroy_command_buffer(*this);
}
