#pragma once
#include "../vulkan/command.hpp"
#include "resource_use.hpp"
#include <vulkan/vulkan_core.h>

namespace VSL_NAMESPACE {
    namespace lifetime {
        struct CommandRecordingContext;
    }
    // Phase内の一つの記録先を保存する値。Workへの新しい所有辺は作らない。
    class NativeRecordingContext {
        CommandBuffer _buffer;
        Completion _completion;
        lifetime::CommandRecordingContext* _context = nullptr;
        impl::SubmissionImpl* _result = nullptr;
        VkCommandBuffer _commands = VK_NULL_HANDLE;
        std::uint32_t _slot = 0;
        std::uint64_t _generation = 0;

    public:
        explicit NativeRecordingContext(CommandBuffer buffer);
        NativeRecordingContext(const NativeRecordingContext&) = default;
        NativeRecordingContext(NativeRecordingContext&&) noexcept = default;
        NativeRecordingContext& operator=(const NativeRecordingContext&) = delete;
        NativeRecordingContext& operator=(NativeRecordingContext&&) = delete;
        Completion completion() const { return _completion; }
        std::uint32_t slot() const { return _slot; }
        void require_current() const;
        // useの全ownerとretainのownerは、このcontextやPhase/Managerを強く所有しない資源境界とする。
        void use(ResourceUse resource, ResourceRetention retention = ResourceRetention::COMPLETION) const;
        void retain(lifetime::OwnerRef owner, ResourceRetention retention = ResourceRetention::COMPLETION) const;
        // 借用値。取得からvkCmd*完了までpollやPhase/ring操作を挟まず、host側で直列化する。
        // 使用登録とは別に、可視性・layout・queue依存のbarrierを呼出側で記録する。
        // 渡すNative資源の同Device・usage・render pass scopeの妥当性も呼出側で保証する。
        VkCommandBuffer native_command_buffer() const&;
        VkCommandBuffer native_command_buffer() const&& = delete;
    };
} // namespace VSL_NAMESPACE
