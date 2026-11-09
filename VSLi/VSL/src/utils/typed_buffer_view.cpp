#include <VSL/utils/typed_buffer_view.hpp>
#include <VSL/lifetime/buffer_arena.hpp>
#include <VSL/lifetime/buffer_storage.hpp>
#include <VSL/vulkan/commands/copy_buffer.hpp>
#include <VSL/vulkan/phase.hpp>
#include <array>
#include <utility>

namespace VSL_NAMESPACE::typed::detail {
    namespace {
        template<buffer_storage_policy Policy>
        BufferView make_uploaded_storage(LogicalDeviceAccessor device, CommandManager manager, const BufferCreateInfo& info,
                                         std::span<const std::byte> bytes, Policy policy);
        template<buffer_storage_policy Policy>
        BufferView make_uploaded_storage(LogicalDeviceAccessor device, CommandManager manager, const BufferCreateInfo& info,
                                         std::span<const std::byte> bytes, Policy policy) {
            if (!device.data_ || !manager.data_ || !manager.data_.use_count() || manager.get_device() != device)
                throw std::invalid_argument("Typed upload requires a command manager on the buffer Device");
            if (bytes.empty() || bytes.size() != info.bytes)
                throw std::invalid_argument("Typed upload requires the complete buffer contents");
            auto target = make_buffer_storage(device, info, policy);
            auto staging = make_buffer_storage(device, {info.bytes, MemoryType::TRANSFER_SOURCE, MemoryProperty::HOST_VISIBLE | MemoryProperty::HOST_COHERENT}, policy);
            staging.write(bytes);
            auto transfer = manager.independent(1);
            auto phase = transfer.start_phase<ComputePhase>();
            phase << command::CopyBuffer(staging, target, bytes.size());
            if (phase.submit() != ComputePhase::SubmitResult::SUCCESS || !phase.completion().wait())
                throw std::runtime_error("Typed buffer upload did not complete");
            return target;
        }
    } // namespace
    BufferView make_buffer_storage(LogicalDeviceAccessor device, const BufferCreateInfo& info, HeapStorage) {
        const auto owner = lifetime::make_heap_buffer(device, info);
        return {{owner.get()}, owner, 0, info.bytes};
    }
    BufferView make_buffer_storage(LogicalDeviceAccessor device, const BufferCreateInfo& info, ArenaStorage<>) {
        const std::array inputs{info};
        return lifetime::BufferArena(device, inputs).view(0);
    }
    BufferView make_buffer_storage(LogicalDeviceAccessor device, CommandManager manager, const BufferCreateInfo& info,
                                   std::span<const std::byte> bytes, HeapStorage policy) {
        return make_uploaded_storage(std::move(device), std::move(manager), info, bytes, policy);
    }
    BufferView make_buffer_storage(LogicalDeviceAccessor device, CommandManager manager, const BufferCreateInfo& info,
                                   std::span<const std::byte> bytes, ArenaStorage<> policy) {
        return make_uploaded_storage(std::move(device), std::move(manager), info, bytes, policy);
    }
    void validate_buffer_view(const BufferAccessor& buffer, MemoryType usage, size_t offset, size_t count, size_t stride, size_t alignment) {
        if (!buffer.data_)
            throw std::invalid_argument("Typed buffer view requires a buffer");
        validate_buffer_view(BufferView::FromBuffer(buffer), usage, offset, count, stride, alignment);
    }
    void validate_buffer_view(const BufferView& buffer, MemoryType usage, size_t offset, size_t count, size_t stride, size_t alignment) {
        if (!contain(buffer.ref().usage(), usage))
            throw std::invalid_argument("Typed buffer view requires matching buffer usage");
        if (!count || offset > buffer.size() || !stride || count > (buffer.size() - offset) / stride)
            throw std::out_of_range("Typed buffer view exceeds requested storage");
        if (!alignment || (buffer.byte_offset() + offset) % alignment)
            throw std::invalid_argument("Typed buffer view offset violates payload alignment");
    }
} // namespace VSL_NAMESPACE::typed::detail
