#include <VSL/lifetime/resource_plan.hpp>
#include <VSL/lifetime/resource_arena.hpp>

#ifndef VSL_TYPED_CASE
#define VSL_TYPED_CASE 0
#endif

namespace {
    namespace t = vsl::typed;
    constexpr auto KIND = vsl::storage::ResourceKind::BUFFER;
    constexpr std::array<t::ResourceRequest, 1> REQUESTS{{{{1}, KIND}}};
#if VSL_TYPED_CASE == 1
    constexpr std::array<t::ResourceRequest, 2> DUPLICATE{{{{1}, KIND}, {{1}, KIND}}};
    constexpr t::ResourcePlan<2> INVALID(DUPLICATE);
#elif VSL_TYPED_CASE == 2
    constexpr t::ResourcePlan<1> INVALID(REQUESTS, {t::LifetimeScope::SHARED, 1});
#elif VSL_TYPED_CASE == 3
    constexpr std::array<t::ResourceRequest, 1> UNKNOWN{{{{1}, static_cast<vsl::storage::ResourceKind>(99)}}};
    constexpr t::ResourcePlan<1> INVALID(UNKNOWN);
#elif VSL_TYPED_CASE == 4
    constexpr t::ResourcePlan<0> INVALID(REQUESTS);
#elif VSL_TYPED_CASE == 5
    auto INVALID = t::ResourcePlan<1>(REQUESTS).placements();
#elif VSL_TYPED_CASE == 6
    constexpr t::ResourcePlan<1> PLAN(REQUESTS);
    auto INVALID = std::move(PLAN).placements();
#elif VSL_TYPED_CASE == 7
    [[maybe_unused]] void invalid(vsl::lifetime::ResourceArena& arena) {
        auto ref = std::move(arena).borrow_buffer({1});
    }
#elif VSL_TYPED_CASE == 8
    [[maybe_unused]] void invalid(const vsl::lifetime::ResourceArena& arena) {
        auto ref = std::move(arena).borrow_buffer({1});
    }
#else
    constexpr t::ResourcePlan<1> PLAN(REQUESTS);
    static_assert(PLAN.placement({1}).offset == 0);
    static_assert(PLAN.bytes() == vsl::storage::BufferLayout::STRIDE);
    static_assert(sizeof(vsl::impl::ImageImpl*) == sizeof(void*));
    [[maybe_unused]] void valid(vsl::LogicalDeviceAccessor device) {
        const std::array<vsl::lifetime::ResourceInit, 1> inputs{vsl::BufferCreateInfo{16, vsl::MemoryType::TRANSFER_SOURCE}};
        vsl::lifetime::ResourceArena arena(device, PLAN, inputs);
        auto ref = arena.borrow_buffer({1});
        auto view = arena.buffer({1});
    }
#endif
} // namespace
