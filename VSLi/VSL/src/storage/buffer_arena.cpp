#include <VSL/storage/buffer_arena.hpp>
#include <VSL/lifetime/buffer_storage.hpp>
#include <VSL/storage/cpu_region.hpp>

namespace VSL_NAMESPACE::storage {
    namespace {
        struct ArenaOwner {
            LogicalDeviceAccessor device;
            std::shared_ptr<detail::CPURegion> memory;
            std::byte* base;
            size_t constructed = 0;
            ArenaOwner(LogicalDeviceAccessor device, std::span<const BufferCreateInfo> buffers,
                       std::shared_ptr<detail::CPURegion> memory);
            ~ArenaOwner();
            void clear() noexcept;
        };

        ArenaOwner::ArenaOwner(LogicalDeviceAccessor device, std::span<const BufferCreateInfo> buffers,
                               std::shared_ptr<detail::CPURegion> memory)
            : device(device), memory(std::move(memory)), base(this->memory ? this->memory->base : nullptr) {
            const auto bytes = BufferLayout::Bytes(buffers.size());
            if (!this->memory)
                throw std::invalid_argument("CPU arena memory is empty");
            this->memory->claim(bytes, BufferLayout::ALIGNMENT);
            try {
                for (const auto& info : buffers) {
                    if (!contain(info.properties, MemoryProperty::HOST_VISIBLE))
                        throw std::invalid_argument("Placed buffers must be host-visible");
                    lifetime::construct_buffer(base + BufferLayout::Offset(constructed), device, info);
                    ++constructed;
                }
            } catch (...) {
                clear();
                throw;
            }
        }
        void ArenaOwner::clear() noexcept {
            while (constructed)
                lifetime::destroy_buffer(base + BufferLayout::Offset(--constructed));
            memory->release();
            base = nullptr;
        }
        ArenaOwner::~ArenaOwner() {
            clear();
        }
    } // namespace

    BufferArena::BufferArena(LogicalDeviceAccessor device, std::span<const BufferCreateInfo> buffers,
                             std::shared_ptr<detail::CPURegion> memory) {
        detail::require_buffer_layout(std::integral_constant<std::uint64_t, BUFFER_LAYOUT_ABI>{});
        auto owner = std::make_shared<ArenaOwner>(device, buffers, std::move(memory));
        _base = owner->base;
        _count = owner->constructed;
        _owner = std::move(owner);
    }
    BufferRef BufferArena::borrow(size_t slot) const& {
        if (!_owner || slot >= _count)
            throw std::out_of_range("Buffer arena slot is not constructed");
        return lifetime::buffer_at(_base + BufferLayout::Offset(slot));
    }
    BufferView BufferArena::view(size_t slot) const {
        const auto ref = borrow(slot);
        return {ref, _owner, 0, ref.size()};
    }
} // namespace VSL_NAMESPACE::storage
