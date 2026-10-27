#include <VSL/utils/typed_resource_allocation.hpp>
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
} // namespace
