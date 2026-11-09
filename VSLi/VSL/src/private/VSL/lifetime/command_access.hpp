#pragma once
#include <VSL/vulkan/command.hpp>
#include <utility>

namespace VSL_NAMESPACE::lifetime {
    struct CommandDomain;
    // Rawは借用、Dataは必要な境界でだけcopyして所有snapshotを作る。
    struct CommandAccess {
        static impl::CommandPoolImpl* Raw(const CommandPool& pool) { return pool._data.get(); }
        static impl::CommandBufferImpl* Raw(const CommandBuffer& buffer) { return buffer._data.get(); }
        static impl::CommandManagerImpl* Raw(const CommandManager& manager) { return manager._data.get(); }
        static const std::shared_ptr<impl::CommandPoolImpl>& Data(const CommandPool& pool) { return pool._data; }
        static const std::shared_ptr<impl::CommandBufferImpl>& Data(const CommandBuffer& buffer) { return buffer._data; }
        static const std::shared_ptr<impl::CommandManagerImpl>& Data(const CommandManager& manager) { return manager._data; }
        static std::shared_ptr<impl::CommandPoolImpl>& Data(CommandPool& pool) { return pool._data; }
        static std::shared_ptr<impl::CommandBufferImpl>& Data(CommandBuffer& buffer) { return buffer._data; }
        static std::shared_ptr<impl::CommandManagerImpl>& Data(CommandManager& manager) { return manager._data; }
        static CommandPool Pool(std::shared_ptr<impl::CommandPoolImpl> pool) { return CommandPool(std::move(pool)); }
        static CommandBuffer RawBuffer(std::shared_ptr<impl::CommandBufferImpl> buffer) { return CommandBuffer(std::move(buffer)); }
        static CommandManager RawManager(std::shared_ptr<impl::CommandManagerImpl> manager) { return CommandManager(std::move(manager)); }
        static const std::shared_ptr<CommandDomain>& Owner(const CommandBuffer& buffer);
        static std::shared_ptr<CommandDomain> Resolve(const std::shared_ptr<impl::CommandBufferImpl>& buffer);
        using Factory = std::shared_ptr<CommandDomain> (*)(std::shared_ptr<impl::CommandBufferImpl>);
        static std::shared_ptr<CommandDomain> CreateDomain(std::shared_ptr<impl::CommandBufferImpl> buffer);
        static CommandBuffer Attach(std::shared_ptr<impl::CommandBufferImpl> buffer, Factory factory = CreateDomain);
        static CommandBuffer Buffer(const std::shared_ptr<CommandDomain>& domain);
        static CommandManager Manager(std::shared_ptr<impl::CommandManagerImpl> data, std::shared_ptr<CommandDomain> domain);
    };
} // namespace VSL_NAMESPACE::lifetime
