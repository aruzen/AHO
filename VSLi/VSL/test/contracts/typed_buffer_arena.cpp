#include <VSL/utils/typed_buffer_arena.hpp>
#include <VSL/vulkan/phase.hpp>
#ifndef VSL_TYPED_CASE
#define VSL_TYPED_CASE 0
#endif

namespace {
    namespace t = vsl::typed;
    constexpr auto USAGE = vsl::MemoryType::TRANSFER_SOURCE | vsl::MemoryType::TRANSFER_DESTINATION;
    using Arena = t::TypedBufferArena<t::BufferSlot<0, 0, std::uint32_t, USAGE>,
                                      t::BufferSlot<0, 1, std::uint32_t, USAGE>>;
    using View = t::PlacedBufferView<std::uint32_t, USAGE>;
    [[maybe_unused]] void contract(vsl::LogicalDeviceAccessor device, vsl::CommandManager manager) {
#if VSL_TYPED_CASE == 1
        Arena arena(device, Arena::Plan({4, 4}));
        const std::array<float, 4> values{};
        arena.view<0, 0>().copy(values);
#elif VSL_TYPED_CASE == 2
        Arena arena(device, Arena::Plan({4, 4}));
        vsl::Buffer<USAGE, vsl::MemoryProperty::HOST_VISIBLE> output(device, 16);
        auto other = t::PlacedBufferView<float, USAGE>::FromBuffer(output);
        auto copy = arena.view<0, 0>().copy_to(other);
#elif VSL_TYPED_CASE == 3
        using Source = t::PlacedBufferView<std::uint32_t, vsl::MemoryType::UNIFORM_BUFFER>;
        vsl::Buffer<vsl::MemoryType::UNIFORM_BUFFER, vsl::MemoryProperty::HOST_VISIBLE> input(device, 16);
        Arena arena(device, Arena::Plan({4, 4}));
        auto copy = Source::FromBuffer(input).copy_to(arena.view<0, 1>());
#elif VSL_TYPED_CASE == 4
        Arena arena(device, Arena::Plan({4, 4}));
        auto missing = arena.view<0, 2>();
#elif VSL_TYPED_CASE == 5
        auto dangling = vsl::storage::BufferArena(device, {}).borrow(0);
#elif VSL_TYPED_CASE == 6
        using Duplicate = t::BufferArenaPlan<t::BufferSlot<0, 0, std::uint32_t, USAGE>,
                                             t::BufferSlot<0, 0, std::uint32_t, USAGE>>;
        constexpr Duplicate duplicate({1, 1});
#else
        constexpr Arena::Plan plan({4, 4});
        Arena arena(device, plan);
        const std::array<std::uint32_t, 4> values{1, 2, 3, 4};
        arena.view<0, 0>().copy(values);
        auto copy = arena.view<0, 0>().copy_to(arena.view<0, 1>());
        auto phase = manager.start_phase<vsl::ComputePhase>();
        phase << copy;
#endif
    }
} // namespace
