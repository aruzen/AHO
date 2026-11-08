#include <VSL/utils/typed_resource_allocation.hpp>
#include <VSL/lifetime/typed_bundle_plan.hpp>
#include <gtest/gtest.h>

namespace {
    namespace t = vsl::typed;
    using First = t::DescriptorSet<0, t::UniformBinding<0, float, vsl::ShaderFlag::VERTEX, 2>, t::StorageBinding<3, std::uint32_t, vsl::ShaderFlag::VERTEX>>;
    using Shared = t::DescriptorSet<4, t::UniformBinding<1, float, vsl::ShaderFlag::VERTEX>>;
    using Images = t::DescriptorSet<7, t::TextureBinding<0, vsl::ShaderFlag::FRAGMENT, 2>>;
    using Resources = t::ResourceSchema<First, Shared, Images>;
    template<typename Elements>
    using Schema = t::ResourceAllocationSchema<Resources,
                                               t::ExternalAllocation<7, 0>, t::BufferAllocation<0, 3, Elements>,
                                               t::BufferAllocation<4, 1, t::FixedElements<2>, t::ResourceScope::SHARED>,
                                               t::BufferAllocation<0, 0, t::FixedElements<1>, t::ResourceScope::SHARED>>;
    using Fixed = Schema<t::FixedElements<5>>;
    using Dynamic = Schema<t::RuntimeElements>;
    constexpr t::ResourceAllocationPlan<Fixed> FIXED_PLAN(3);
    static_assert(FIXED_PLAN.totals().buffers == 6 && FIXED_PLAN.totals().descriptor_sets == 5);
    static_assert(FIXED_PLAN.totals().descriptors == 12 && FIXED_PLAN.totals().payload_bytes == 76);
    static_assert(std::same_as<Fixed::Binding<0, 0>::Payload, float>);

    TEST(TypedAllocationPlanTest, CountsDescriptorArraysAndSparseSetsWithSharing) {
        EXPECT_EQ(FIXED_PLAN.frame_count(), 3u);
        const auto shared = FIXED_PLAN.entry<0, 0>();
        EXPECT_EQ(shared.instances, 1u);
        EXPECT_EQ(shared.buffers, 2u);
        EXPECT_EQ(shared.payload_bytes, 8u);
        const auto local = FIXED_PLAN.entry<0, 3>();
        EXPECT_EQ(local.elements, 5u);
        EXPECT_EQ(local.buffers, 3u);
        EXPECT_EQ(local.payload_bytes, 60u);
        const auto external = FIXED_PLAN.entry<7, 0>();
        EXPECT_EQ(external.buffers, 0u);
        EXPECT_EQ(external.payload_bytes, 0u);
        EXPECT_TRUE(Fixed::SetIsPerFrame(0));
        EXPECT_FALSE(Fixed::SetIsPerFrame(4));
        EXPECT_EQ(Fixed::SetIndex(7), 2u);
    }

    TEST(TypedAllocationPlanTest, RuntimeElementsAreExplicitAndCopiedIntoThePlan) {
        t::ResourceAllocationSizes<Dynamic> sizes;
        EXPECT_THROW(t::ResourceAllocationPlan<Dynamic>(3, sizes), std::invalid_argument);
        EXPECT_THROW((sizes.set<0, 3>(0)), std::invalid_argument);
        sizes.set<0, 3>(5);
        const t::ResourceAllocationPlan<Dynamic> plan(3, sizes);
        EXPECT_EQ(plan.totals().payload_bytes, FIXED_PLAN.totals().payload_bytes);
        sizes.set<0, 3>(10);
        EXPECT_EQ((plan.entry<0, 3>().elements), 5u);
        const t::ResourceAllocationPlan<Dynamic> larger(4, sizes);
        EXPECT_EQ(larger.totals().buffers, 7u);
        EXPECT_EQ(larger.totals().descriptor_sets, 6u);
        EXPECT_EQ(larger.totals().descriptors, 15u);
        EXPECT_EQ(larger.totals().payload_bytes, 176u);
    }

    TEST(TypedAllocationPlanTest, RejectsZeroFramesAndUnknownLookups) {
        EXPECT_THROW(t::ResourceAllocationPlan<Fixed>(0), std::invalid_argument);
        EXPECT_THROW(Fixed::Index(0, 7), std::invalid_argument);
        EXPECT_THROW(Fixed::SetIndex(5), std::invalid_argument);
    }

    TEST(TypedAllocationPlanTest, DetectsElementMultiplicationAndTotalOverflowWithoutAllocating) {
        t::ResourceAllocationSizes<Dynamic> sizes;
        sizes.set<0, 3>(std::numeric_limits<size_t>::max());
        EXPECT_THROW(t::ResourceAllocationPlan<Dynamic>(1, sizes), std::overflow_error);
        EXPECT_THROW((t::ResourceAllocationPlan<Fixed>{std::numeric_limits<size_t>::max()}), std::overflow_error);
        using Huge = t::ResourceAllocationSchema<t::ResourceSchema<t::DescriptorSet<0,
                                                                                    t::StorageBinding<0, std::uint32_t, vsl::ShaderFlag::COMPUTE>, t::StorageBinding<1, std::uint32_t, vsl::ShaderFlag::COMPUTE>>>,
                                                 t::BufferAllocation<0, 0, t::RuntimeElements, t::ResourceScope::SHARED>, t::BufferAllocation<0, 1, t::RuntimeElements, t::ResourceScope::SHARED>>;
        t::ResourceAllocationSizes<Huge> huge;
        huge.set<0, 0>(std::numeric_limits<size_t>::max() / 4).set<0, 1>(1);
        EXPECT_THROW(t::ResourceAllocationPlan<Huge>(1, huge), std::overflow_error);
    }

    TEST(TypedAllocationPlanTest, EmptySchemasNeedNoNativeAllocations) {
        using Empty = t::ResourceAllocationSchema<t::ResourceSchema<>>;
        constexpr t::ResourceAllocationPlan<Empty> plan(3);
        EXPECT_EQ(plan.totals().buffers, 0u);
        EXPECT_EQ(plan.totals().descriptor_sets, 0u);
        EXPECT_EQ(plan.totals().payload_bytes, 0u);
    }

    TEST(TypedAllocationPlanTest, ArenaPlanSeparatesLifetimeGroupsAndExcludesExternalSlots) {
        constexpr t::TypedBundlePlan<Fixed> plan(FIXED_PLAN);
        static_assert(plan.buffers(t::ResourceScope::SHARED).size() == 3);
        static_assert(plan.buffers(t::ResourceScope::PER_FRAME).size() == 1);
        static_assert(plan.total_bytes() == vsl::storage::BufferLayout::Bytes(6));
        EXPECT_TRUE((plan.placement({7, 0, 1}).external));
        EXPECT_EQ((plan.placement({7, 0, 1}).slot), SIZE_MAX);
        EXPECT_NE((plan.placement({0, 0, 0}).slot), (plan.placement({0, 0, 1}).slot));
        EXPECT_EQ(plan.alignment(t::ResourceScope::SHARED), vsl::storage::BufferLayout::ALIGNMENT);
        EXPECT_THROW(plan.placement({4, 1, 2}), std::invalid_argument);
    }

    TEST(TypedAllocationPlanTest, ExplicitResourceIdentitySharesSlotsWithoutEncodingBindingNumbers) {
        using Map = t::BufferBindingMap<t::ResourceBinding<t::BindingKey{0, 0, 0}, t::ResourceId{0}>,
                                        t::ResourceBinding<t::BindingKey{0, 0, 1}, t::ResourceId{0}>>;
        constexpr t::TypedBundlePlan<Fixed, Map> plan(FIXED_PLAN);
        static_assert(plan.buffers(t::ResourceScope::SHARED).size() == 2);
        EXPECT_EQ((plan.placement({0, 0, 0}).resource), (t::ResourceId{0}));
        EXPECT_EQ((plan.placement({0, 0, 0}).slot), (plan.placement({0, 0, 1}).slot));
        EXPECT_NE((plan.placement({4, 1, 0}).resource), (t::ResourceId{0}));
    }

    TEST(TypedAllocationPlanTest, ArenaPlanRejectsConflictingDeclarationsAndUnknownMapKeys) {
        using Conflict = t::BufferBindingMap<t::ResourceBinding<t::BindingKey{0, 0, 0}, t::ResourceId{0}>,
                                             t::ResourceBinding<t::BindingKey{4, 1, 0}, t::ResourceId{0}>>;
        EXPECT_THROW((t::TypedBundlePlan<Fixed, Conflict>(FIXED_PLAN)), std::invalid_argument);
        using Unknown = t::BufferBindingMap<t::ResourceBinding<t::BindingKey{123, 0}, t::ResourceId{0}>>;
        EXPECT_THROW((t::TypedBundlePlan<Fixed, Unknown>(FIXED_PLAN)), std::invalid_argument);
        using Empty = t::ResourceAllocationSchema<t::ResourceSchema<>>;
        constexpr t::TypedBundlePlan<Empty> empty(t::ResourceAllocationPlan<Empty>(1));
        static_assert(empty.total_bytes() == 0 && empty.alignment(t::ResourceScope::SHARED) == 1);
    }
    TEST(TypedAllocationPlanTest, DescriptorArenaPlanPreservesDataOffsetsAndCountsDeclaredSets) {
        constexpr t::TypedBundlePlan<Fixed> plan(FIXED_PLAN);
        constexpr auto shared = plan.arena_plan(t::ResourceScope::SHARED);
        constexpr auto frame = plan.arena_plan(t::ResourceScope::PER_FRAME, 2);
        static_assert(shared.size() == 9 && shared.slots() == 9);
        static_assert(frame.size() == 4 && frame.slots() == 3);
        static_assert(frame.lifetime() == t::LifetimeGroup{t::LifetimeScope::FRAME, 2});
        static_assert(frame.placement(plan.descriptor_layout(0)).external);
        static_assert(frame.placement(plan.descriptor_layout(0)).slot == t::ResourcePlacement::NO_SLOT);
        static_assert(shared.placement(plan.descriptor_layout(7)).kind == vsl::storage::ResourceKind::DESCRIPTOR_LAYOUT);
        static_assert(plan.total_arena_bytes() == shared.bytes() + 3 * frame.bytes());
        EXPECT_GT(plan.total_arena_bytes(), plan.total_bytes());
        for (const auto scope : {t::ResourceScope::SHARED, t::ResourceScope::PER_FRAME}) {
            const auto full = plan.arena_plan(scope);
            const auto data = plan.resource_plan(scope);
            for (const auto& previous : data.placements()) {
                const auto current = full.placement(previous.resource);
                EXPECT_EQ(current.slot, previous.slot);
                EXPECT_EQ(current.offset, previous.offset);
                EXPECT_EQ(current.kind, previous.kind);
            }
            EXPECT_EQ(full.placement(*plan.descriptor_pool(scope)).kind, vsl::storage::ResourceKind::DESCRIPTOR_POOL);
        }
        EXPECT_EQ(shared.placement(plan.descriptor_set(7)).kind, vsl::storage::ResourceKind::DESCRIPTOR_SET);
        EXPECT_THROW(plan.descriptor_layout(6), std::out_of_range);
        EXPECT_THROW(plan.descriptor_set(6), std::out_of_range);
        EXPECT_THROW(plan.arena_plan(t::ResourceScope::SHARED, 1), std::invalid_argument);
        EXPECT_THROW(plan.descriptor_pool(static_cast<t::ResourceScope>(99)), std::invalid_argument);
    }
    TEST(TypedAllocationPlanTest, DescriptorIdsAvoidExplicitExternalAndMaximumResourceIds) {
        using Map = t::ResourceBindingMap<t::ResourceBinding<t::BindingKey{0, 0, 0}, t::ResourceId{UINT64_MAX}>,
                                          t::ResourceBinding<t::BindingKey{7, 0, 0}, t::ResourceId{0}>,
                                          t::ResourceBinding<t::BindingKey{4, 1}, t::ResourceId{1}>>;
        constexpr t::TypedBundlePlan<Fixed, Map> plan(FIXED_PLAN);
        constexpr auto shared = plan.arena_plan(t::ResourceScope::SHARED);
        constexpr auto frame = plan.arena_plan(t::ResourceScope::PER_FRAME);
        static_assert(plan.placement({0, 0, 0}).resource == t::ResourceId{UINT64_MAX});
        static_assert(plan.descriptor_layout(0) != t::ResourceId{0});
        std::vector<t::ResourceId> descriptors;
        for (const auto& declaration : plan.descriptors().Layouts()) {
            descriptors.push_back(plan.descriptor_layout(declaration.set));
            descriptors.push_back(plan.descriptor_set(declaration.set));
        }
        descriptors.push_back(*plan.descriptor_pool(t::ResourceScope::SHARED));
        descriptors.push_back(*plan.descriptor_pool(t::ResourceScope::PER_FRAME));
        for (size_t i = 0; i < descriptors.size(); ++i) {
            for (const auto& data : plan.placements())
                EXPECT_NE(descriptors[i], data.resource);
            for (size_t j = 0; j < i; ++j)
                EXPECT_NE(descriptors[i], descriptors[j]);
        }
        EXPECT_TRUE(frame.placement(plan.descriptor_layout(0)).external);
        EXPECT_FALSE(shared.placement(plan.descriptor_layout(0)).external);
    }
    TEST(TypedAllocationPlanTest, EmptySchemaAndDeclaredEmptySetNeedDifferentArenaStorage) {
        using Empty = t::ResourceAllocationSchema<t::ResourceSchema<>>;
        constexpr t::TypedBundlePlan<Empty> empty(t::ResourceAllocationPlan<Empty>(3));
        static_assert(empty.total_arena_bytes() == 0);
        static_assert(!empty.descriptor_pool(t::ResourceScope::SHARED));
        static_assert(!empty.descriptor_pool(t::ResourceScope::PER_FRAME));
        static_assert(empty.arena_plan(t::ResourceScope::SHARED).slots() == 0);
        using Declared = t::ResourceAllocationSchema<t::ResourceSchema<t::DescriptorSet<500>>>;
        constexpr t::TypedBundlePlan<Declared> declared(t::ResourceAllocationPlan<Declared>(3));
        static_assert(declared.total_bytes() == 0 && declared.total_arena_bytes() > 0);
        static_assert(declared.arena_plan(t::ResourceScope::SHARED).slots() == 3);
        static_assert(declared.arena_plan(t::ResourceScope::PER_FRAME).size() == 0);
        static_assert(!declared.descriptor_pool(t::ResourceScope::PER_FRAME));
        EXPECT_THROW(empty.descriptor_layout(0), std::out_of_range);
        EXPECT_THROW(empty.descriptor_set(0), std::out_of_range);
    }

    TEST(TypedAllocationPlanTest, EqualPayloadSizeDoesNotPermitAliasingDifferentTypes) {
        using Two = t::ResourceAllocationSchema<t::ResourceSchema<t::DescriptorSet<0,
                                                                                   t::UniformBinding<0, float, vsl::ShaderFlag::COMPUTE>, t::UniformBinding<1, std::uint32_t, vsl::ShaderFlag::COMPUTE>>>,
                                                t::BufferAllocation<0, 0>, t::BufferAllocation<0, 1>>;
        using Map = t::BufferBindingMap<t::ResourceBinding<t::BindingKey{0, 0}, t::ResourceId{1}>, t::ResourceBinding<t::BindingKey{0, 1}, t::ResourceId{1}>>;
        EXPECT_THROW((t::TypedBundlePlan<Two, Map>(t::ResourceAllocationPlan<Two>(1))), std::invalid_argument);
    }

    TEST(TypedAllocationPlanTest, ArenaCpuByteOverflowIsIndependentOfPayloadByteOverflow) {
        using Bytes = t::ResourceAllocationSchema<t::ResourceSchema<t::DescriptorSet<0, t::UniformBinding<0, std::uint8_t, vsl::ShaderFlag::COMPUTE>>>, t::BufferAllocation<0, 0>>;
        const t::ResourceAllocationPlan<Bytes> logical(SIZE_MAX);
        EXPECT_EQ(logical.totals().payload_bytes, SIZE_MAX);
        EXPECT_THROW((t::TypedBundlePlan<Bytes>(logical)), std::overflow_error);
    }
    using Type = vsl::graphic_resource::Type;
    constexpr auto COMPUTE = vsl::ShaderFlag::COMPUTE;
    constexpr auto IMAGE = t::ResourceComponent::IMAGE, SAMPLER = t::ResourceComponent::SAMPLER;
    constexpr vsl::ImageCreateInfo IMAGE_INFO{vsl::ImageType::SAMPLED | vsl::ImageType::TRANSFER_DST, 3, 2, vsl::data_format::UNSIGNED_NORMALIZED8_RGBA};
    using Mixed = t::ResourceAllocationSchema<t::ResourceSchema<t::DescriptorSet<0, t::TextureBinding<0, COMPUTE>, t::DescriptorBinding<1, Type::SAMPLED_IMAGE, COMPUTE>,
                                                                                 t::DescriptorBinding<2, Type::SAMPLER, COMPUTE>, t::StorageBinding<3, std::uint32_t, COMPUTE>>>,
                                              t::TextureAllocation<0, 0>, t::ImageAllocation<0, 1>, t::SamplerAllocation<0, 2>, t::BufferAllocation<0, 3>>;
    constexpr auto MIXED_SIZES = [] { t::ResourceAllocationSizes<Mixed> sizes; sizes.image<0, 0>(IMAGE_INFO).image<0, 1>(IMAGE_INFO); return sizes; }();
    using SharedComponents = t::ResourceBindingMap<t::ResourceBinding<t::BindingKey{0, 0}, t::ResourceId{12}, IMAGE>, t::ResourceBinding<t::BindingKey{0, 1}, t::ResourceId{12}>,
                                                   t::ResourceBinding<t::BindingKey{0, 0}, t::ResourceId{13}, SAMPLER>, t::ResourceBinding<t::BindingKey{0, 2}, t::ResourceId{13}>>;
    TEST(TypedAllocationPlanTest, MixedComponentsHaveIndependentCountsScopesAndPhysicalPlacements) {
        constexpr t::ResourceAllocationPlan<Mixed> logical(3, MIXED_SIZES);
        constexpr t::TypedBundlePlan<Mixed, SharedComponents> plan(logical);
        static_assert(logical.totals().buffers == 3 && logical.totals().images == 6 && logical.totals().samplers == 2);
        static_assert(logical.totals().payload_bytes == 12 && logical.totals().descriptor_sets == 3 && logical.totals().descriptors == 12);
        static_assert(plan.resources(t::ResourceScope::SHARED).size() == 1 && plan.resources(t::ResourceScope::PER_FRAME).size() == 2);
        static_assert(plan.placement({0, 0}, IMAGE).slot == plan.placement({0, 1}).slot);
        static_assert(plan.placement({0, 0}, SAMPLER).slot == plan.placement({0, 2}).slot);
        EXPECT_EQ(plan.resource_plan(t::ResourceScope::PER_FRAME, 2).lifetime(), (t::LifetimeGroup{t::LifetimeScope::FRAME, 2}));
        EXPECT_EQ(plan.placement({0, 1}).kind, vsl::storage::ResourceKind::IMAGE);
        EXPECT_NE(plan.placement({0, 1}).resource, plan.placement({0, 2}).resource);
        const auto physical = plan.resource_plan(t::ResourceScope::PER_FRAME);
        EXPECT_EQ(plan.placement({0, 3}).offset, physical.placement(plan.placement({0, 3}).resource).offset);
        EXPECT_EQ(plan.bytes(t::ResourceScope::PER_FRAME), physical.bytes());
        EXPECT_EQ(plan.total_bytes(), plan.bytes(t::ResourceScope::SHARED) + 3 * plan.bytes(t::ResourceScope::PER_FRAME));
    }
    TEST(TypedAllocationPlanTest, ImageCreationInputsAreRequiredAndDescriptorArraysKeepTheirOwnInfos) {
        EXPECT_THROW((t::ResourceAllocationPlan<Mixed>(2)), std::invalid_argument);
        auto invalid = MIXED_SIZES;
        auto info = IMAGE_INFO;
        info.usage = vsl::ImageType::TRANSFER_DST;
        invalid.image<0, 0>(info);
        EXPECT_THROW((t::ResourceAllocationPlan<Mixed>(2, invalid)), std::invalid_argument);
        using Arrays = t::ResourceAllocationSchema<t::ResourceSchema<t::DescriptorSet<0, t::TextureBinding<0, COMPUTE, 2>>>, t::TextureAllocation<0, 0>>;
        t::ResourceAllocationSizes<Arrays> sizes;
        auto other = IMAGE_INFO;
        other.width = 7;
        sizes.image<0, 0>(std::array{IMAGE_INFO, other});
        const t::ResourceAllocationPlan<Arrays> logical(2, sizes);
        const t::TypedBundlePlan<Arrays> plan(logical);
        EXPECT_EQ(std::get<vsl::ImageCreateInfo>(plan.resources(t::ResourceScope::PER_FRAME)[0]).width, 3u);
        EXPECT_EQ(std::get<vsl::ImageCreateInfo>(plan.resources(t::ResourceScope::PER_FRAME)[1]).width, 7u);
        EXPECT_EQ(logical.totals().images, 4u);
        EXPECT_EQ(logical.totals().samplers, 2u);
    }
    TEST(TypedAllocationPlanTest, SharedIdsRejectDifferentCreationInfosAndPhysicalKinds) {
        auto sizes = MIXED_SIZES;
        auto info = IMAGE_INFO;
        info.width = 7;
        sizes.image<0, 1>(info);
        EXPECT_THROW((t::TypedBundlePlan<Mixed, SharedComponents>(t::ResourceAllocationPlan<Mixed>(2, sizes))), std::invalid_argument);
        sizes = MIXED_SIZES;
        sizes.sampler<0, 2>({vsl::SamplingMode::NEAREST, vsl::EdgeMode::CLAMP_TO_EDGE});
        EXPECT_THROW((t::TypedBundlePlan<Mixed, SharedComponents>(t::ResourceAllocationPlan<Mixed>(2, sizes))), std::invalid_argument);
        using WrongKind = t::ResourceBindingMap<t::ResourceBinding<t::BindingKey{0, 0}, t::ResourceId{0}, IMAGE>, t::ResourceBinding<t::BindingKey{0, 0}, t::ResourceId{0}, SAMPLER>>;
        EXPECT_THROW((t::TypedBundlePlan<Mixed, WrongKind>(t::ResourceAllocationPlan<Mixed>(2, MIXED_SIZES))), std::invalid_argument);
    }
    TEST(TypedAllocationPlanTest, SharingSampledAndStorageImagesRequiresAnExplicitLayoutPolicy) {
        using Images = t::ResourceAllocationSchema<t::ResourceSchema<t::DescriptorSet<0, t::DescriptorBinding<0, Type::SAMPLED_IMAGE, COMPUTE>, t::DescriptorBinding<1, Type::STORAGE_IMAGE, COMPUTE>>>, t::ImageAllocation<0, 0>, t::ImageAllocation<0, 1>>;
        t::ResourceAllocationSizes<Images> sizes;
        auto info = IMAGE_INFO;
        info.usage = info.usage | vsl::ImageType::STORAGE;
        sizes.image<0, 0>(info).image<0, 1>(info);
        using Map = t::ResourceBindingMap<t::ResourceBinding<t::BindingKey{0, 0}, t::ResourceId{1}>, t::ResourceBinding<t::BindingKey{0, 1}, t::ResourceId{1}>>;
        EXPECT_THROW((t::TypedBundlePlan<Images, Map>(t::ResourceAllocationPlan<Images>(1, sizes))), std::invalid_argument);
    }
    TEST(TypedAllocationPlanTest, ExternalImageAndSamplerMetadataNeverAllocateOwnedSlots) {
        using External = t::ResourceAllocationSchema<t::ResourceSchema<t::DescriptorSet<0, t::DescriptorBinding<0, Type::SAMPLED_IMAGE, COMPUTE>, t::DescriptorBinding<1, Type::SAMPLER, COMPUTE>>>, t::ExternalAllocation<0, 0>, t::ExternalAllocation<0, 1>>;
        constexpr t::TypedBundlePlan<External> plan(t::ResourceAllocationPlan<External>(3));
        static_assert(plan.placement({0, 0}).kind == vsl::storage::ResourceKind::IMAGE);
        static_assert(plan.placement({0, 1}).kind == vsl::storage::ResourceKind::SAMPLER);
        static_assert(plan.placement({0, 0}).slot == SIZE_MAX && plan.placement({0, 1}).slot == SIZE_MAX);
        static_assert(plan.total_bytes() == 0);
        EXPECT_TRUE(plan.resources(t::ResourceScope::SHARED).empty());
    }
} // namespace
