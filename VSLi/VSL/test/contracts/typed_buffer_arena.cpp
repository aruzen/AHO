#include <VSL/utils/typed_buffer_arena.hpp>
#include <VSL/storage/buffer_arena.hpp>
#include <VSL/vulkan/buffer_view.hpp>
#include <VSL/vulkan/buffer_create_info.hpp>
#include <VSL/vulkan/phase.hpp>
#ifndef VSL_TYPED_CASE
#define VSL_TYPED_CASE 0
#endif

namespace {
    namespace t = vsl::typed;
    static_assert(std::is_same_v<vsl::storage::BufferArena, vsl::lifetime::BufferArena>);
    constexpr auto USAGE = vsl::MemoryType::TRANSFER_SOURCE | vsl::MemoryType::TRANSFER_DESTINATION;
    using Arena = t::TypedBufferArena<t::BufferResource<t::ResourceId{10}, std::uint32_t, USAGE>,
                                      t::BufferResource<t::ResourceId{20}, std::uint32_t, USAGE>>;
    using View = t::PlacedBufferView<std::uint32_t, USAGE>;
    [[maybe_unused]] void contract(vsl::LogicalDeviceAccessor device, vsl::CommandManager manager) {
#if VSL_TYPED_CASE == 1
        Arena arena(device, Arena::Plan({4, 4}));
        const std::array<float, 4> values{};
        arena.view<t::ResourceId{10}>().copy(values);
#elif VSL_TYPED_CASE == 2
        Arena arena(device, Arena::Plan({4, 4}));
        vsl::Buffer<USAGE, vsl::MemoryProperty::HOST_VISIBLE> output(device, 16);
        auto other = t::PlacedBufferView<float, USAGE>::FromBuffer(output);
        auto copy = arena.view<t::ResourceId{10}>().copy_to(other);
#elif VSL_TYPED_CASE == 3
        using Source = t::PlacedBufferView<std::uint32_t, vsl::MemoryType::UNIFORM_BUFFER>;
        vsl::Buffer<vsl::MemoryType::UNIFORM_BUFFER, vsl::MemoryProperty::HOST_VISIBLE> input(device, 16);
        Arena arena(device, Arena::Plan({4, 4}));
        auto copy = Source::FromBuffer(input).copy_to(arena.view<t::ResourceId{20}>());
#elif VSL_TYPED_CASE == 4
        Arena arena(device, Arena::Plan({4, 4}));
        auto missing = arena.view<t::ResourceId{30}>();
#elif VSL_TYPED_CASE == 5
        auto dangling = vsl::storage::BufferArena(device, {}).borrow(0);
#elif VSL_TYPED_CASE == 6
        using Duplicate = t::BufferArenaPlan<t::BufferResource<t::ResourceId{10}, std::uint32_t, USAGE>,
                                             t::BufferResource<t::ResourceId{10}, std::uint32_t, USAGE>>;
        constexpr Duplicate duplicate({1, 1});
#elif VSL_TYPED_CASE == 7
        using Mixed = t::BufferArenaPlan<t::BufferResource<t::ResourceId{1}, std::uint32_t, USAGE>,
                                         t::BufferResource<t::ResourceId{2}, std::uint32_t, USAGE, t::LifetimeScope::FRAME>>;
        Mixed mixed({1, 1});
#elif VSL_TYPED_CASE == 8
        Arena arena(device, Arena::Plan({4, 4}));
        auto escaped = arena.view<t::ResourceId{10}>().read_bytes();
#elif VSL_TYPED_CASE == 9
        using Map = t::BufferBindingMap<t::ResourceBinding<t::BindingKey{0, 0}, t::ResourceId{10}>,
                                        t::ResourceBinding<t::BindingKey{0, 0}, t::ResourceId{20}>>;
        constexpr auto id = Map::Resolve<t::BindingKey{0, 0}>();
#elif VSL_TYPED_CASE == 10
        using Map = t::BufferBindingMap<t::ResourceBinding<t::BindingKey{0, 0}, t::ResourceId{10}>>;
        constexpr auto id = Map::Resolve<t::BindingKey{0, 1}>();
#elif VSL_TYPED_CASE == 11
        using Map = t::BufferBindingMap<t::ResourceBinding<t::BindingKey{0, 0}, t::ResourceId{30}>>;
        Arena arena(device, Arena::Plan({4, 4}));
        auto invalid = arena.binding_view<Map, t::BindingKey{0, 0}>();
#elif VSL_TYPED_CASE == 12
        Arena arena(device, Arena::Plan({4, 4}));
        std::array<float, 4> invalid{};
        arena.view<t::ResourceId{10}>().read_into(invalid);
#elif VSL_TYPED_CASE == 13
        auto dangling = Arena(device, Arena::Plan({4, 4})).storage().borrow(0);
#elif VSL_TYPED_CASE == 14
        const Arena arena(device, Arena::Plan({4, 4}));
        auto dangling = std::move(arena).storage().borrow(0);
#else
        constexpr Arena::Plan plan({4, 4});
        Arena arena(device, plan);
        const Arena& immutable = arena;
        auto borrowed = immutable.storage().borrow(0);
        auto owned = Arena(device, plan).view<t::ResourceId{10}>();
        const std::array<std::uint32_t, 4> values{1, 2, 3, 4};
        arena.view<t::ResourceId{10}>().copy(values);
        auto copy = arena.view<t::ResourceId{10}>().copy_to(arena.view<t::ResourceId{20}>());
        auto phase = manager.start_phase<vsl::ComputePhase>();
        phase << copy;
#endif
    }
} // namespace
