#pragma once
#include "../storage/buffer_arena.hpp"
#include "../vulkan/commands/copy_buffer.hpp"
#include "typed_block_layout.hpp"
#include "typed_buffer_view.hpp"
#include <tuple>

namespace VSL_NAMESPACE::typed {
    template<typename Payload_, MemoryType usage_>
    class PlacedBufferView {
        BufferView _view;

    public:
        using Payload = typename PayloadLayout<Payload_>::Payload;
        static_assert(std::is_trivially_copyable_v<Payload> && std::is_standard_layout_v<Payload>);
        static constexpr auto USAGE = usage_;
        explicit PlacedBufferView(BufferView view);
        static PlacedBufferView FromBuffer(const BufferAccessor& buffer) { return PlacedBufferView(BufferView::FromBuffer(buffer)); }
        size_t size() const { return _view.size() / sizeof(Payload); }
        const BufferView& backend() const { return _view; }
        PlacedBufferView view(size_t first, size_t count) const;
        void copy(std::span<const Payload> values) const;
        std::span<const std::byte> read_bytes() const { return _view.read(_view.size()); }
        template<MemoryType destination_usage>
        requires(contain<usage_, MemoryType::TRANSFER_SOURCE>() && contain<destination_usage, MemoryType::TRANSFER_DESTINATION>())
        command::CopyBuffer copy_to(const PlacedBufferView<Payload_, destination_usage>& destination, bool host_read = true) const;
    };

    template<std::uint32_t set, std::uint32_t binding, typename Payload_, MemoryType usage,
             size_t element = 0, size_t frame = 0>
    struct BufferSlot {
        using PayloadTag = Payload_;
        using Payload = typename PayloadLayout<Payload_>::Payload;
        static constexpr auto USAGE = usage;
        static constexpr std::array<size_t, 4> KEY{set, binding, element, frame};
        static_assert(std::is_trivially_copyable_v<Payload> && std::is_standard_layout_v<Payload>);
    };

    template<typename... Slots>
    struct BufferArenaPlan {
        static constexpr size_t COUNT = sizeof...(Slots);
        static constexpr size_t ALIGNMENT = COUNT ? storage::BufferLayout::ALIGNMENT : 1;
        static constexpr size_t BYTES = storage::BufferLayout::Bytes(COUNT);
        static constexpr std::array<std::array<size_t, 4>, COUNT> KEYS{Slots::KEY...};
        static_assert([] {
            for (size_t i = 0; i < COUNT; ++i)
                for (size_t j = 0; j < i; ++j)
                    if (KEYS[i] == KEYS[j])
                        return false;
            return true;
        }(),
                      "Duplicate typed buffer slot key");
        std::array<BufferCreateInfo, COUNT> buffers;
        constexpr explicit BufferArenaPlan(const std::array<size_t, COUNT>& counts);
        static constexpr size_t Offset(size_t index);
        template<std::uint32_t set, std::uint32_t binding, size_t element = 0, size_t frame = 0>
        static consteval size_t Index();
    };

    template<typename... Slots>
    class TypedBufferArena {
        storage::BufferArena _storage;

    public:
        using Plan = BufferArenaPlan<Slots...>;
        explicit TypedBufferArena(LogicalDeviceAccessor device, const Plan& plan) : _storage(device, plan.buffers) {}
        TypedBufferArena(LogicalDeviceAccessor device, const Plan& plan, storage::ArenaMemory memory)
            : _storage(device, plan.buffers, std::move(memory)) {}
        const storage::BufferArena& storage() const { return _storage; }
        template<std::uint32_t set, std::uint32_t binding, size_t element = 0, size_t frame = 0>
        auto view() const;
    };

    template<typename Payload_, MemoryType usage>
    PlacedBufferView<Payload_, usage>::PlacedBufferView(BufferView view) : _view(std::move(view)) {
        if (!contain(_view.ref().usage(), usage))
            throw std::invalid_argument("Typed placed buffer usage mismatch");
        if (_view.size() % sizeof(Payload) || _view.byte_offset() % alignof(Payload))
            throw std::invalid_argument("Typed placed buffer requires aligned whole payloads");
    }
    template<typename Payload_, MemoryType usage>
    PlacedBufferView<Payload_, usage> PlacedBufferView<Payload_, usage>::view(size_t first, size_t count) const {
        const auto offset = detail::checked_view_offset(0, first, count, size(), sizeof(Payload));
        return PlacedBufferView(_view.view(offset, count * sizeof(Payload)));
    }
    template<typename Payload_, MemoryType usage>
    void PlacedBufferView<Payload_, usage>::copy(std::span<const Payload> values) const {
        if (values.size() != size())
            throw std::invalid_argument("Typed placed buffer copy must preserve record count");
        _view.write(std::as_bytes(values));
    }
    template<typename Payload_, MemoryType usage>
    template<MemoryType destination_usage>
    requires(contain<usage, MemoryType::TRANSFER_SOURCE>() && contain<destination_usage, MemoryType::TRANSFER_DESTINATION>())
    command::CopyBuffer PlacedBufferView<Payload_, usage>::copy_to(const PlacedBufferView<Payload_, destination_usage>& destination, bool host_read) const {
        if (size() != destination.size())
            throw std::invalid_argument("Typed buffer copy requires equal record counts");
        return command::CopyBuffer(_view, destination.backend(), _view.size(), 0, 0, host_read);
    }
    template<typename... Slots>
    constexpr BufferArenaPlan<Slots...>::BufferArenaPlan(const std::array<size_t, COUNT>& counts) {
        size_t index = 0;
        buffers = {BufferCreateInfo{helper::checked_buffer_array_size<typename Slots::Payload>(counts[index++]), Slots::USAGE}...};
    }
    template<typename... Slots>
    constexpr size_t BufferArenaPlan<Slots...>::Offset(size_t index) {
        if (index >= COUNT)
            throw std::out_of_range("Typed buffer slot index");
        return storage::BufferLayout::Offset(index);
    }
    template<typename... Slots>
    template<std::uint32_t set, std::uint32_t binding, size_t element, size_t frame>
    consteval size_t BufferArenaPlan<Slots...>::Index() {
        for (size_t i = 0; i < COUNT; ++i)
            if (KEYS[i] == std::array<size_t, 4>{set, binding, element, frame})
                return i;
        throw "Unknown typed buffer slot";
    }
    template<typename... Slots>
    template<std::uint32_t set, std::uint32_t binding, size_t element, size_t frame>
    auto TypedBufferArena<Slots...>::view() const {
        constexpr auto index = Plan::template Index<set, binding, element, frame>();
        using Slot = std::tuple_element_t<index, std::tuple<Slots...>>;
        return PlacedBufferView<typename Slot::PayloadTag, Slot::USAGE>(_storage.view(index));
    }
} // namespace VSL_NAMESPACE::typed
