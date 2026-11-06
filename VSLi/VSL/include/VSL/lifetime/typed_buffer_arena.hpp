#pragma once
#include "buffer_arena.hpp"
#include "../vulkan/commands/copy_buffer.hpp"
#include "../utils/typed_block_layout.hpp"
#include "../utils/typed_buffer_view.hpp"
#include <tuple>
#include <compare>

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
        void read_into(std::span<Payload> destination) const;
        template<MemoryType destination_usage>
        requires(contain<usage_, MemoryType::TRANSFER_SOURCE>() && contain<destination_usage, MemoryType::TRANSFER_DESTINATION>())
        command::CopyBuffer copy_to(const PlacedBufferView<Payload_, destination_usage>& destination, bool host_read = true) const;
    };

    struct ResourceId {
        std::uint64_t value;
        constexpr auto operator<=>(const ResourceId&) const = default;
    };
    struct BindingKey {
        std::uint32_t set, binding;
        size_t element = 0;
        constexpr auto operator<=>(const BindingKey&) const = default;
    };
    enum class LifetimeScope {
        SHARED,
        FRAME,
        TRANSIENT
    };
    struct LifetimeGroup {
        LifetimeScope scope;
        size_t instance;
        constexpr auto operator<=>(const LifetimeGroup&) const = default;
    };
    struct BufferPlacement {
        ResourceId resource;
        size_t slot, offset;
    };
    template<BindingKey key, ResourceId resource>
    struct ResourceBinding {
        static constexpr auto KEY = key;
        static constexpr auto RESOURCE = resource;
    };
    template<typename... Bindings>
    struct BufferBindingMap {
        static constexpr std::array<BindingKey, sizeof...(Bindings)> KEYS{Bindings::KEY...};
        static constexpr std::array<ResourceId, sizeof...(Bindings)> RESOURCES{Bindings::RESOURCE...};
        static_assert([] {
            for (size_t i = 0; i < KEYS.size(); ++i)
                for (size_t j = 0; j < i; ++j)
                    if (KEYS[i] == KEYS[j])
                        return false;
            return true;
        }(),
                      "Duplicate buffer binding key");
        template<BindingKey key>
        static consteval ResourceId Resolve();
    };

    // 資源の宣言。bindingや物理slot番号はこのIDに含めない。
    template<ResourceId id, typename Payload_, MemoryType usage, LifetimeScope scope = LifetimeScope::SHARED>
    struct BufferResource {
        using PayloadTag = Payload_;
        using Payload = typename PayloadLayout<Payload_>::Payload;
        static constexpr auto USAGE = usage;
        static constexpr auto ID = id;
        static constexpr auto SCOPE = scope;
        static_assert(std::is_trivially_copyable_v<Payload> && std::is_standard_layout_v<Payload>);
    };

    template<typename... Slots>
    struct BufferArenaPlan {
        static constexpr size_t COUNT = sizeof...(Slots);
        static constexpr size_t ALIGNMENT = COUNT ? storage::BufferLayout::ALIGNMENT : 1;
        static constexpr size_t BYTES = storage::BufferLayout::Bytes(COUNT);
        static constexpr std::array<ResourceId, COUNT> IDS{Slots::ID...};
        static constexpr std::array<LifetimeScope, COUNT> SCOPES{Slots::SCOPE...};
        static constexpr auto SCOPE = COUNT ? SCOPES[0] : LifetimeScope::SHARED;
        static_assert(((Slots::SCOPE == SCOPE) && ...), "A buffer arena must contain one lifetime scope");
        static_assert([] {
            for (size_t i = 0; i < COUNT; ++i)
                for (size_t j = 0; j < i; ++j)
                    if (IDS[i] == IDS[j])
                        return false;
            return true;
        }(),
                      "Duplicate buffer resource ID");
        static constexpr std::array<BufferPlacement, COUNT> PLACEMENTS = [] {
            std::array<BufferPlacement, COUNT> placements{};
            for (size_t i = 0; i < COUNT; ++i)
                placements[i] = {IDS[i], i, storage::BufferLayout::Offset(i)};
            return placements;
        }();
        std::array<BufferCreateInfo, COUNT> buffers;
        constexpr explicit BufferArenaPlan(const std::array<size_t, COUNT>& counts, size_t lifetime_instance = 0);
        constexpr LifetimeGroup lifetime() const { return _lifetime; }
        static constexpr size_t Offset(size_t index);
        template<ResourceId id>
        static consteval size_t Index();

    private:
        LifetimeGroup _lifetime;
    };

    template<typename... Slots>
    class TypedBufferArena {
        lifetime::BufferArena _storage;
        LifetimeGroup _lifetime;

    public:
        using Plan = BufferArenaPlan<Slots...>;
        explicit TypedBufferArena(LogicalDeviceAccessor device, const Plan& plan) : _storage(device, plan.buffers), _lifetime(plan.lifetime()) {}
        TypedBufferArena(LogicalDeviceAccessor device, const Plan& plan, storage::ArenaMemory memory)
            : _storage(device, plan.buffers, std::move(memory)), _lifetime(plan.lifetime()) {}
        const lifetime::BufferArena& storage() const& { return _storage; }
        const lifetime::BufferArena& storage() const&& = delete;
        LifetimeGroup lifetime() const { return _lifetime; }
        template<ResourceId id>
        auto view() const;
        template<typename Bindings, BindingKey key>
        auto binding_view() const { return view<Bindings::template Resolve<key>()>(); }
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
    void PlacedBufferView<Payload_, usage>::read_into(std::span<Payload> destination) const {
        if (destination.size() != size())
            throw std::invalid_argument("Typed placed buffer read must preserve record count");
        _view.read_into(std::as_writable_bytes(destination));
    }
    template<typename... Bindings>
    template<BindingKey key>
    consteval ResourceId BufferBindingMap<Bindings...>::Resolve() {
        for (size_t i = 0; i < KEYS.size(); ++i)
            if (KEYS[i] == key)
                return RESOURCES[i];
        throw "Unknown buffer binding key";
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
    constexpr BufferArenaPlan<Slots...>::BufferArenaPlan(const std::array<size_t, COUNT>& counts, size_t lifetime_instance)
        : _lifetime{SCOPE, lifetime_instance} {
        if (SCOPE == LifetimeScope::SHARED && lifetime_instance)
            throw std::invalid_argument("Shared buffer lifetime instance must be zero");
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
    template<ResourceId id>
    consteval size_t BufferArenaPlan<Slots...>::Index() {
        for (size_t i = 0; i < COUNT; ++i)
            if (IDS[i] == id)
                return i;
        throw "Unknown buffer resource ID";
    }
    template<typename... Slots>
    template<ResourceId id>
    auto TypedBufferArena<Slots...>::view() const {
        constexpr auto index = Plan::template Index<id>();
        using Slot = std::tuple_element_t<index, std::tuple<Slots...>>;
        return PlacedBufferView<typename Slot::PayloadTag, Slot::USAGE>(_storage.view(index));
    }
} // namespace VSL_NAMESPACE::typed
