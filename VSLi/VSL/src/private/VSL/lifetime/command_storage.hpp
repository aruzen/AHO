#pragma once
#include <VSL/operation/command_ops.hpp>
#include <VSL/lifetime/owner_ref.hpp>

namespace VSL_NAMESPACE::lifetime {
    struct CommandBufferOwnerContext {
        WeakOwnedRef<impl::CommandPoolImpl> pool;
        std::weak_ptr<CommandDomain> domain;
    };
    struct CommandPoolSlotStorage {
        alignas(impl::CommandPoolImpl) std::byte value[sizeof(impl::CommandPoolImpl)];
    };
    struct CommandBufferSlotStorage {
        alignas(impl::CommandBufferImpl) std::byte value[sizeof(impl::CommandBufferImpl)];
        alignas(CommandBufferOwnerContext) std::byte context[sizeof(CommandBufferOwnerContext)];
    };
    void construct_command_pool(void* storage, LogicalDeviceAccessor device, CommandPoolCreateInfo info);
    void construct_command_pool(void* storage, impl::LogicalDeviceImpl& device, CommandPoolCreateInfo info);
    void destroy_command_pool(void* storage) noexcept;
    impl::CommandPoolImpl& command_pool_at(void* storage);
    void construct_command_buffer(void* storage, impl::CommandPoolImpl& pool, std::uint32_t count);
    void destroy_command_buffer(void* storage) noexcept;
    impl::CommandBufferImpl& command_buffer_at(void* storage);
    void set_command_buffer_pool_owner(impl::CommandBufferImpl& buffer, OwnedRef<impl::CommandPoolImpl> pool);
    std::shared_ptr<impl::CommandPoolImpl> make_heap_command_pool(LogicalDeviceAccessor device, CommandPoolCreateInfo info);
    std::shared_ptr<impl::CommandBufferImpl> make_heap_command_buffer(CommandPool pool, std::uint32_t count);
} // namespace VSL_NAMESPACE::lifetime
