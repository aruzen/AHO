#include <VSL/lifetime/typed_descriptor_plan.hpp>
#include <VSL/lifetime/typed_bundle_plan.hpp>
#include <VSL/utils/typed_resource_bundle.hpp>
#include <gtest/gtest.h>

namespace {
    namespace t = vsl::typed;
    using Type = vsl::graphic_resource::Type;
    using Scope = t::ResourceScope;
    using Local = t::DescriptorSet<9, t::UniformBinding<3, float, vsl::ShaderFlag::COMPUTE, 2>,
                                   t::StorageBinding<6, std::uint32_t, vsl::ShaderFlag::COMPUTE, 3>>;
    using EmptySet = t::DescriptorSet<7>;
    using Images = t::DescriptorSet<40, t::TextureBinding<2, vsl::ShaderFlag::FRAGMENT, 2>>;
    using Shared = t::DescriptorSet<2, t::UniformBinding<0, float, vsl::ShaderFlag::VERTEX>>;
    using Resources = t::ResourceSchema<Local, EmptySet, Images, Shared>;
    using Schema = t::ResourceAllocationSchema<Resources,
                                               t::BufferAllocation<9, 3, t::FixedElements<1>, Scope::SHARED>,
                                               t::BufferAllocation<9, 6>, t::ExternalAllocation<40, 2>,
                                               t::BufferAllocation<2, 0, t::FixedElements<1>, Scope::SHARED>>;
    constexpr t::ResourceAllocationPlan<Schema> ALLOCATION(3);
    constexpr t::TypedDescriptorPlan<Schema> PLAN(ALLOCATION);
    static_assert(PLAN.Layouts().size() == 4 && Resources::SET_COUNT == 41);
    static_assert(PLAN.total_sets() == 6 && PLAN.total_pools() == 4 && PLAN.total_descriptors() == 18);
    static_assert(PLAN.pool(Scope::SHARED).max_sets == 3 && PLAN.pool(Scope::PER_FRAME).max_sets == 1);
    static_assert(PLAN.total_sets() == ALLOCATION.totals().descriptor_sets);
    static_assert(PLAN.total_descriptors() == ALLOCATION.totals().descriptors);
    template<typename P>
    concept temporary_sets = requires(P&& plan) { std::move(plan).sets(Scope::SHARED); };
    template<typename P>
    concept temporary_pool = requires(P&& plan) { std::move(plan).pool(Scope::SHARED); };
    template<typename P>
    concept temporary_descriptors = requires(P&& plan) { std::move(plan).descriptors(); };
    template<typename P>
    concept temporary_placement = requires(P&& bundle) { std::move(bundle).placement_plan(); };
    static_assert(!temporary_sets<t::TypedDescriptorPlan<Schema>>);
    static_assert(!temporary_pool<t::TypedDescriptorPlan<Schema>>);
    static_assert(!temporary_descriptors<t::TypedBundlePlan<Schema>>);
    static_assert(!temporary_placement<t::TypedResourceBundle<Schema, t::ArenaStorage<>>>);
    std::uint32_t capacity(t::DescriptorPoolPlan pool, Type type);
    std::uint32_t capacity(t::DescriptorPoolPlan pool, Type type) {
        for (const auto& entry : pool.descriptors)
            if (entry.type == type)
                return entry.count;
        return 0;
    }

    TEST(TypedDescriptorPlanTest, SparseSetNumbersUseDeclaredLayoutsAndStableLayoutOrdinals) {
        const auto layouts = PLAN.Layouts();
        EXPECT_EQ(layouts[0].set, 9u);
        EXPECT_EQ(layouts[1].set, 7u);
        EXPECT_EQ(layouts[2].set, 40u);
        EXPECT_EQ(layouts[3].set, 2u);
        EXPECT_EQ(layouts[0].bindings.data(), Local::POINTS.data());
        EXPECT_TRUE(layouts[1].bindings.empty());
        EXPECT_EQ(PLAN.sets(Scope::PER_FRAME).size(), 1u);
        EXPECT_EQ(PLAN.sets(Scope::PER_FRAME)[0].layout, 0u);
        const auto shared = PLAN.sets(Scope::SHARED);
        ASSERT_EQ(shared.size(), 3u);
        EXPECT_EQ(shared[0].set, 7u);
        EXPECT_EQ(shared[0].layout, 1u);
        EXPECT_EQ(shared[1].layout, 2u);
        EXPECT_EQ(shared[2].layout, 3u);
    }
    TEST(TypedDescriptorPlanTest, PoolCapacityCountsEveryBindingOfTheSetIncludingSharedAndExternalValues) {
        const auto local = PLAN.pool(Scope::PER_FRAME);
        EXPECT_EQ(capacity(local, Type::UNIFORM_BUFFER), 2u);
        EXPECT_EQ(capacity(local, Type::STORAGE_BUFFER), 3u);
        EXPECT_EQ(capacity(local, Type::COMBINED_IMAGE_SAMPLER), 0u);
        const auto shared = PLAN.pool(Scope::SHARED);
        EXPECT_EQ(capacity(shared, Type::UNIFORM_BUFFER), 1u);
        EXPECT_EQ(capacity(shared, Type::COMBINED_IMAGE_SAMPLER), 2u);
        EXPECT_EQ(capacity(shared, Type::SAMPLER), 0u);
        EXPECT_EQ(local.descriptors.size(), 2u);
        EXPECT_EQ(shared.descriptors.size(), 2u);
        EXPECT_EQ(PLAN.frame_count(), 3u);
    }
    TEST(TypedDescriptorPlanTest, BundleKeepsTheConstexprDescriptorPlanAlongsideItsDataPlacement) {
        constexpr t::TypedBundlePlan<Schema> bundle(ALLOCATION);
        static_assert(bundle.descriptors().total_sets() == ALLOCATION.totals().descriptor_sets);
        static_assert(bundle.descriptors().total_descriptors() == ALLOCATION.totals().descriptors);
        EXPECT_EQ(bundle.descriptors().Layouts().size(), Resources::COUNT);
        EXPECT_EQ(bundle.resource_plan(Scope::PER_FRAME).slots(), 3u);
        EXPECT_EQ(bundle.resource_plan(Scope::SHARED).slots(), 3u);
        using Map = t::ResourceBindingMap<t::ResourceBinding<t::BindingKey{9, 3, 0}, t::ResourceId{42}>,
                                          t::ResourceBinding<t::BindingKey{9, 3, 1}, t::ResourceId{42}>>;
        constexpr t::TypedBundlePlan<Schema, Map> shared_storage(ALLOCATION);
        static_assert(shared_storage.resource_plan(Scope::SHARED).slots() == 2);
        EXPECT_EQ(shared_storage.descriptors().total_descriptors(), PLAN.total_descriptors());
        EXPECT_EQ(capacity(shared_storage.descriptors().pool(Scope::PER_FRAME), Type::UNIFORM_BUFFER), 2u);
    }
    TEST(TypedDescriptorPlanTest, EmptySchemaAndDeclaredEmptySetHaveDifferentNativeBudgets) {
        using None = t::ResourceAllocationSchema<t::ResourceSchema<>>;
        constexpr t::TypedDescriptorPlan<None> none(3);
        static_assert(none.total_sets() == 0 && none.total_pools() == 0);
        EXPECT_TRUE(none.Layouts().empty());
        EXPECT_EQ(none.pool(Scope::SHARED).max_sets, 0u);
        EXPECT_EQ(none.pool(Scope::PER_FRAME).max_sets, 0u);
        using Declared = t::ResourceAllocationSchema<t::ResourceSchema<EmptySet>>;
        constexpr t::TypedDescriptorPlan<Declared> declared(3);
        static_assert(declared.total_sets() == 1 && declared.total_pools() == 1 && declared.total_descriptors() == 0);
        EXPECT_EQ(declared.Layouts().size(), 1u);
        EXPECT_EQ(declared.pool(Scope::SHARED).max_sets, 1u);
        EXPECT_TRUE(declared.pool(Scope::SHARED).descriptors.empty());
        EXPECT_EQ(declared.pool(Scope::PER_FRAME).max_sets, 0u);
    }
    TEST(TypedDescriptorPlanTest, RuntimeFramesScaleTotalsWithoutMultiplyingPerPoolCapacity) {
        const t::TypedDescriptorPlan<Schema> many(1000000);
        EXPECT_EQ(many.total_sets(), 1000003u);
        EXPECT_EQ(many.total_pools(), 1000001u);
        EXPECT_EQ(many.total_descriptors(), 5000003u);
        EXPECT_EQ(capacity(many.pool(Scope::PER_FRAME), Type::STORAGE_BUFFER), 3u);
        EXPECT_EQ(many.pool(Scope::PER_FRAME).max_sets, 1u);
        EXPECT_EQ(many.sets(Scope::PER_FRAME).size(), 1u);
    }
    TEST(TypedDescriptorPlanTest, RejectsInvalidFramesScopeAndNativeOrTotalOverflow) {
        EXPECT_THROW(t::TypedDescriptorPlan<Schema>(0), std::invalid_argument);
        EXPECT_THROW(PLAN.sets(static_cast<Scope>(99)), std::invalid_argument);
        EXPECT_THROW(PLAN.pool(static_cast<Scope>(99)), std::invalid_argument);
        EXPECT_THROW(t::TypedDescriptorPlan<Schema>(SIZE_MAX), std::overflow_error);
        using Huge = t::ResourceAllocationSchema<t::ResourceSchema<t::DescriptorSet<0,
                                                                                    t::StorageBinding<0, std::uint32_t, vsl::ShaderFlag::COMPUTE, UINT32_MAX>,
                                                                                    t::StorageBinding<1, std::uint32_t, vsl::ShaderFlag::COMPUTE>>>,
                                                 t::ExternalAllocation<0, 0>, t::ExternalAllocation<0, 1>>;
        EXPECT_THROW(t::TypedDescriptorPlan<Huge>(1), std::out_of_range);
        using Pair = t::ResourceAllocationSchema<t::ResourceSchema<t::DescriptorSet<0,
                                                                                    t::StorageBinding<0, std::uint32_t, vsl::ShaderFlag::COMPUTE, 2>>>,
                                                 t::BufferAllocation<0, 0>>;
        EXPECT_THROW(t::TypedDescriptorPlan<Pair>(SIZE_MAX), std::overflow_error);
    }
} // namespace
