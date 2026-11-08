#include <VSL/lifetime/buffer_arena.hpp>
#include <VSL/lifetime/buffer_storage.hpp>
#include <VSL/storage/cpu_region.hpp>
#include <VSL/lifetime/dependency_node.hpp>
#include <VSL/lifetime/device_owner.hpp>

namespace VSL_NAMESPACE::lifetime {
    namespace {
        struct ArenaOwner {
            DependencyNode node;
            LogicalDeviceAccessor device;
            std::shared_ptr<storage::detail::CPURegion> memory;
            std::byte* base;
            size_t constructed = 0;
            ArenaOwner(LogicalDeviceAccessor device, std::span<const BufferCreateInfo> buffers,
                       std::shared_ptr<storage::detail::CPURegion> memory);
            ~ArenaOwner();
            void clear() noexcept;
            static void VisitDependencies(const void* owner, DependencyNode::EdgeVisitor visitor, void* context);
        };

        ArenaOwner::ArenaOwner(LogicalDeviceAccessor device, std::span<const BufferCreateInfo> buffers,
                               std::shared_ptr<storage::detail::CPURegion> memory)
            : device(device), memory(std::move(memory)), base(this->memory ? this->memory->base : nullptr) {
            require_device_owner(device);
            const auto bytes = storage::BufferLayout::Bytes(buffers.size());
            if (!this->memory)
                throw std::invalid_argument("CPU arena memory is empty");
            this->memory->claim(bytes, storage::BufferLayout::ALIGNMENT);
            try {
                for (const auto& info : buffers) {
                    if (!contain(info.properties, MemoryProperty::HOST_VISIBLE))
                        throw std::invalid_argument("Placed buffers must be host-visible");
                    lifetime::construct_buffer(base + storage::BufferLayout::Offset(constructed), *device.data_, info);
                    ++constructed;
                }
            } catch (...) {
                clear();
                throw;
            }
        }
        void ArenaOwner::clear() noexcept {
            while (constructed)
                lifetime::destroy_buffer(base + storage::BufferLayout::Offset(--constructed));
            memory->release();
            base = nullptr;
        }
        ArenaOwner::~ArenaOwner() {
            node.retire();
            clear();
        }
        void ArenaOwner::VisitDependencies(const void* owner, DependencyNode::EdgeVisitor visitor, void* context) {
            visitor(context, static_cast<const ArenaOwner*>(owner)->device.owner_ref());
        }
    } // namespace

    BufferArena::BufferArena(LogicalDeviceAccessor device, std::span<const BufferCreateInfo> buffers,
                             std::shared_ptr<storage::detail::CPURegion> memory) {
        storage::detail::require_buffer_layout(std::integral_constant<std::uint64_t, storage::BUFFER_LAYOUT_ABI>{});
        auto owner = std::make_shared<ArenaOwner>(device, buffers, std::move(memory));
        owner->node.publish(owner, owner.get(), ArenaOwner::VisitDependencies);
        for (size_t slot = 0; slot < owner->constructed; ++slot)
            buffer_at(owner->base + storage::BufferLayout::Offset(slot)).data->owner_node = &owner->node;
        _base = owner->base;
        _count = owner->constructed;
        _owner = std::move(owner);
    }
    BufferRef BufferArena::borrow(size_t slot) const& {
        if (!_owner || slot >= _count)
            throw std::out_of_range("Buffer arena slot is not constructed");
        return lifetime::buffer_at(_base + storage::BufferLayout::Offset(slot));
    }
    BufferView BufferArena::view(size_t slot) const {
        const auto ref = borrow(slot);
        return {ref, _owner, 0, ref.size()};
    }
} // namespace VSL_NAMESPACE::lifetime
