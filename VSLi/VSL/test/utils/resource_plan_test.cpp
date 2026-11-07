#include <VSL/lifetime/resource_plan.hpp>
#include <gtest/gtest.h>

namespace {
    namespace t = vsl::typed;
    namespace s = vsl::storage;
    using enum s::ResourceKind;
    constexpr std::array<t::ResourceRequest, 5> REQUESTS{{{{40}, IMAGE, true}, {{10}, BUFFER}, {{20}, IMAGE}, {{50}, SAMPLER, true}, {{30}, BUFFER}}};
    constexpr t::ResourcePlan<8> PLAN(REQUESTS, {t::LifetimeScope::FRAME, 2});
    static_assert(PLAN.size() == 5 && PLAN.slots() == 3);
    static_assert(PLAN.placement({10}).slot == 0 && PLAN.placement({20}).slot == 1 && PLAN.placement({30}).slot == 2);
    static_assert(PLAN.placement({40}).slot == t::ResourcePlacement::NO_SLOT);
    static_assert(PLAN.placement({10}).offset == 0);
    static_assert(PLAN.placement({30}).offset > PLAN.placement({10}).offset);
    static_assert(PLAN.bytes() % PLAN.alignment() == 0);
    static_assert(s::ResourceSlotLayout<vsl::impl::BufferImpl>::SIZE == s::BufferLayout::STRIDE);
    constexpr t::ResourcePlan<0> EMPTY(std::span<const t::ResourceRequest>{});
    static_assert(EMPTY.bytes() == 0 && EMPTY.alignment() == 1 && EMPTY.slots() == 0);

    TEST(ResourcePlanTest, MixedKindsAndRepeatedImplTypesHaveAlignedNonoverlappingSlots) {
        const std::array<t::ResourceRequest, 4> requests{{{{1}, SAMPLER}, {{2}, BUFFER}, {{3}, IMAGE}, {{4}, SAMPLER}}};
        const t::ResourcePlan<4> plan(requests);
        size_t end = 0, index = 0;
        for (const auto& p : plan.placements()) {
            const auto layout = s::resource_slot_layout(p.kind);
            EXPECT_EQ(p.slot, index++);
            EXPECT_EQ(p.offset % layout.alignment, 0u);
            EXPECT_GE(p.offset, end);
            EXPECT_LE(p.offset + layout.size, plan.bytes());
            EXPECT_GE(layout.size, layout.use_offset + sizeof(vsl::ResourceUseState));
            end = p.offset + layout.size;
        }
        EXPECT_EQ(plan.bytes(), s::checked_align(end, plan.alignment()));
        EXPECT_NE(plan.placement({1}).offset, plan.placement({4}).offset);
        EXPECT_EQ(plan.lifetime(), (t::LifetimeGroup{t::LifetimeScope::SHARED, 0}));
    }
    TEST(ResourcePlanTest, RuntimeCountsAreBoundedAndThePlanOwnsItsMetadata) {
        auto requests = REQUESTS;
        const t::ResourcePlan<8> plan(std::span(requests).first(3), {t::LifetimeScope::FRAME, 9});
        requests[1].resource = {999};
        EXPECT_EQ(plan.size(), 3u);
        EXPECT_EQ(plan.placements().size(), 3u);
        EXPECT_EQ(plan.slots(), 2u);
        EXPECT_EQ(plan.placement({10}).kind, BUFFER);
        EXPECT_EQ(plan.lifetime(), (t::LifetimeGroup{t::LifetimeScope::FRAME, 9}));
        EXPECT_THROW(plan.placement({999}), std::out_of_range);
        EXPECT_THROW((t::ResourcePlan<2>(REQUESTS)), std::length_error);
    }
    TEST(ResourcePlanTest, ExternalEntriesHaveNoSlotOrOffsetAndNeedNoCpuRegion) {
        const std::array<t::ResourceRequest, 2> requests{{{{1}, IMAGE, true}, {{2}, SAMPLER, true}}};
        const t::ResourcePlan<2> plan(requests, {t::LifetimeScope::TRANSIENT, 7});
        EXPECT_EQ(plan.bytes(), 0u);
        EXPECT_EQ(plan.alignment(), 1u);
        EXPECT_EQ(plan.slots(), 0u);
        for (const auto& p : plan.placements()) {
            EXPECT_TRUE(p.external);
            EXPECT_EQ(p.slot, t::ResourcePlacement::NO_SLOT);
            EXPECT_EQ(p.offset, t::ResourcePlacement::NO_SLOT);
        }
        EXPECT_NO_THROW(plan.require_region(0, 1));
        EXPECT_EQ(PLAN.placement({20}).slot, 1u);
    }
    TEST(ResourcePlanTest, DuplicateIdsUnknownKindsAndInvalidLifetimeGroupsAreRejected) {
        auto requests = REQUESTS;
        requests[1].resource = requests[0].resource;
        EXPECT_THROW((t::ResourcePlan<8>(requests)), std::invalid_argument);
        requests = REQUESTS;
        requests[0].kind = static_cast<s::ResourceKind>(99);
        EXPECT_THROW((t::ResourcePlan<8>(requests)), std::invalid_argument);
        EXPECT_THROW((t::ResourcePlan<8>(REQUESTS, {t::LifetimeScope::SHARED, 1})), std::invalid_argument);
        EXPECT_THROW((t::ResourcePlan<8>(REQUESTS, {static_cast<t::LifetimeScope>(99), 0})), std::invalid_argument);
        EXPECT_THROW(PLAN.placement({404}), std::out_of_range);
    }
    TEST(ResourcePlanTest, CapacityAlignmentAndArithmeticOverflowAreChecked) {
        EXPECT_NO_THROW(PLAN.require_region(PLAN.bytes(), PLAN.alignment()));
        EXPECT_THROW(PLAN.require_region(PLAN.bytes() - 1, PLAN.alignment()), std::invalid_argument);
        EXPECT_THROW(PLAN.require_region(PLAN.bytes(), 1), std::invalid_argument);
        EXPECT_THROW(PLAN.require_region(PLAN.bytes(), 0), std::invalid_argument);
        EXPECT_THROW(PLAN.require_region(PLAN.bytes(), 3), std::invalid_argument);
        EXPECT_THROW(s::checked_add(SIZE_MAX, 1), std::overflow_error);
        EXPECT_THROW(s::checked_align(SIZE_MAX, 2), std::overflow_error);
        EXPECT_EQ(s::checked_add(SIZE_MAX, 0), SIZE_MAX);
    }
} // namespace
