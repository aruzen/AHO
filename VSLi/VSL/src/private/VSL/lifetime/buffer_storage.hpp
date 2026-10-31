#pragma once
#include <VSL/backend/buffer_ops.hpp>
#include <VSL/vulkan/buffer_view.hpp>

namespace VSL_NAMESPACE::lifetime {
    struct BufferSlotStorage {
        alignas(impl::BufferImpl) std::byte buffer[sizeof(impl::BufferImpl)];
        alignas(ResourceUseState) std::byte use[sizeof(ResourceUseState)];
    };
    // 呼出側が未構築・sizeof/alignof(BufferSlotStorage)以上の領域を予約し、deviceを保持する。
    // 失敗時はnative資源とC++実体を回収し、領域を未構築へ戻す。
    void construct_buffer(void* storage, LogicalDeviceAccessor device, const BufferCreateInfo& info);
    // GPU使用の完了または未送信記録の取消を確認後、構築済みslotへ一度だけ呼ぶ。
    void destroy_buffer(void* storage) noexcept;
    BufferRef buffer_at(void* storage);
    std::shared_ptr<impl::BufferImpl> make_heap_buffer(LogicalDeviceAccessor device, const BufferCreateInfo& info);
} // namespace VSL_NAMESPACE::lifetime
