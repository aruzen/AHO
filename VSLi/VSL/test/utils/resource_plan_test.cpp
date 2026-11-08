#include <VSL/lifetime/resource_plan.hpp>
#include <VSL/lifetime/resource_arena.hpp>
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

    TEST(ResourcePlanTest, PipelineLayoutsUseExactAlignedSlotsWithoutGpuUseState) {
        constexpr std::array<t::ResourceRequest, 3> requests{{{{1}, PIPELINE_LAYOUT}, {{2}, PIPELINE_LAYOUT}, {{3}, PIPELINE_LAYOUT, true}}};
        constexpr t::ResourcePlan<3> plan(requests);
        static_assert(s::ResourceSlotLayout<vsl::impl::PipelineLayoutImpl>::USE_OFFSET == s::NO_SEPARATE_USE);
        static_assert(s::ResourceSlotLayout<vsl::impl::PipelineLayoutImpl>::SIZE == s::PIPELINE_LAYOUT_IMPL_SIZE);
        static_assert(plan.placement({1}).offset != plan.placement({2}).offset);
        EXPECT_EQ(plan.slots(), 2u);
        EXPECT_EQ(plan.bytes(), 2 * s::ResourceSlotLayout<vsl::impl::PipelineLayoutImpl>::SIZE);
        EXPECT_EQ(plan.placement({3}).slot, t::ResourcePlacement::NO_SLOT);
        EXPECT_EQ(plan.placement({2}).offset % s::PIPELINE_LAYOUT_IMPL_ALIGNMENT, 0u);
    }

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
    TEST(ResourcePlanTest, DescriptorSlotsUseExactImplSizesWithoutTrailingUseStates) {
        constexpr std::array<t::ResourceRequest, 7> requests{{{{1}, DESCRIPTOR_SET}, {{2}, BUFFER}, {{3}, DESCRIPTOR_LAYOUT}, {{4}, IMAGE}, {{5}, DESCRIPTOR_POOL}, {{6}, DESCRIPTOR_SET}, {{7}, SAMPLER}}};
        constexpr t::ResourcePlan<7> plan(requests);
        static_assert(plan.slots() == 7 && plan.placement({1}).offset != plan.placement({6}).offset);
        static_assert(s::ResourceSlotLayout<vsl::impl::GraphicResourceImpl>::SIZE == s::ImplLayout<vsl::impl::GraphicResourceImpl>::SIZE);
        static_assert(s::ResourceSlotLayout<vsl::impl::GraphicResourceImpl>::USE_OFFSET == s::NO_SEPARATE_USE);
        size_t end = 0;
        for (const auto& placement : plan.placements()) {
            const auto layout = s::resource_slot_layout(placement.kind);
            EXPECT_EQ(placement.offset % layout.alignment, 0u);
            EXPECT_GE(placement.offset, end);
            EXPECT_LE(placement.offset + layout.size, plan.bytes());
            if (placement.kind == DESCRIPTOR_SET || placement.kind == DESCRIPTOR_LAYOUT || placement.kind == DESCRIPTOR_POOL)
                EXPECT_EQ(layout.use_offset, s::NO_SEPARATE_USE);
            else
                EXPECT_LE(layout.use_offset + sizeof(vsl::ResourceUseState), layout.size);
            end = placement.offset + layout.size;
        }
        EXPECT_EQ(s::resource_slot_layout(DESCRIPTOR_LAYOUT).size, s::DESCRIPTOR_LAYOUT_IMPL_SIZE);
        EXPECT_EQ(s::resource_slot_layout(DESCRIPTOR_POOL).size, s::DESCRIPTOR_POOL_IMPL_SIZE);
        EXPECT_EQ(s::resource_slot_layout(DESCRIPTOR_SET).size, s::DESCRIPTOR_SET_IMPL_SIZE);
    }
    TEST(LogicalUseStateTest, IndependentOwnersMoveSafelyAndExportOwningValuesOnly) {
        namespace l = vsl::lifetime;
        static_assert(!std::constructible_from<l::LogicalUseState, std::shared_ptr<void>>);
        static_assert(!std::constructible_from<l::LogicalUseState, std::shared_ptr<vsl::ResourceUseState>>);
        static_assert(!std::constructible_from<l::ResourceArena, vsl::LogicalDeviceAccessor, const t::ResourcePlan<7>&,
                                               std::span<const l::ResourceInit>, s::ArenaMemory, std::span<const std::shared_ptr<void>>>);
        vsl::ResourceUse saved;
        std::weak_ptr<vsl::ResourceUseState> observed;
        {
            l::LogicalUseState state, other;
            auto alias = state;
            auto moved = std::move(state);
            observed = moved.resource_use().state.share();
            EXPECT_THROW(state.resource_use(), std::logic_error);
            EXPECT_THROW(state.busy(), std::logic_error);
            EXPECT_THROW(state.wait(0), std::logic_error);
            EXPECT_EQ(moved.resource_use().state, alias.resource_use().state);
            EXPECT_NE(moved.resource_use().state, other.resource_use().state);
            EXPECT_FALSE(moved.busy());
            EXPECT_TRUE(moved.wait(0));
            saved = moved.resource_use();
        }
        EXPECT_FALSE(observed.expired());
        saved = {};
        EXPECT_TRUE(observed.expired());
        EXPECT_FALSE(l::LogicalUseState{}.resource_use().state->busy());
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
