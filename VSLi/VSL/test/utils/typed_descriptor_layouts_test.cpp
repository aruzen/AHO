#include <VSL/lifetime/typed_descriptor_layouts.hpp>
#include <VSL/utils/typed_resource_bundle.hpp>
#include <VSL/utils/typed_compute.hpp>
#include <VSL/utils/typed_pipeline.hpp>
#include "support/vulkan_fixture.hpp"

namespace {
    namespace t = vsl::typed;
    namespace pl = vsl::pipeline_layout;
    using One = t::ResourceSchema<t::DescriptorSet<2, t::UniformBinding<3, std::uint32_t, vsl::ShaderFlag::COMPUTE, 2>>>;
    using OneLayouts = t::TypedDescriptorLayouts<One>;
    using Other = t::ResourceSchema<t::DescriptorSet<3, t::UniformBinding<3, std::uint32_t, vsl::ShaderFlag::COMPUTE, 2>>>;
    using Inputs = t::TypedVertexInput<>;
    using LocalSet = t::DescriptorSet<2, t::UniformBinding<0, std::uint32_t, vsl::ShaderFlag::COMPUTE>,
                                      t::UniformBinding<1, std::uint32_t, vsl::ShaderFlag::COMPUTE>,
                                      t::StorageBinding<3, std::uint32_t, vsl::ShaderFlag::COMPUTE>>;
    using SharedSet = t::DescriptorSet<5, t::UniformBinding<0, std::uint32_t, vsl::ShaderFlag::COMPUTE, 2>>;
    using Resources = t::ResourceSchema<LocalSet, SharedSet>;
    using Allocation = t::ResourceAllocationSchema<Resources, t::BufferAllocation<2, 0>,
                                                   t::BufferAllocation<2, 1, t::FixedElements<1>, t::ResourceScope::SHARED>,
                                                   t::BufferAllocation<2, 3, t::RuntimeElements>,
                                                   t::BufferAllocation<5, 0, t::FixedElements<1>, t::ResourceScope::SHARED>>;
    using Bundle = t::TypedResourceBundle<Allocation, t::ArenaStorage<>>;
    static_assert(!vsl::pipeline_layout_injecter<OneLayouts>);
    static_assert(std::constructible_from<t::TypedComputePipeline<One>, vsl::LogicalDeviceAccessor, const OneLayouts&, pl::ShaderGroup>);
    static_assert(!std::constructible_from<t::TypedComputePipeline<Other>, vsl::LogicalDeviceAccessor, const OneLayouts&, pl::ShaderGroup>);
    static_assert(!std::constructible_from<t::TypedGraphicsPipeline<Inputs, Other>, vsl::LogicalDeviceAccessor, vsl::RenderPassAccessor, const OneLayouts&, pl::ShaderGroup>);
    class SharedLayoutsVulkanIntegrationTest : public vsl_test::VulkanIntegrationTest {
        bool validate_vulkan() const override { return true; }
    };
    t::ResourceAllocationSizes<Allocation> bundle_sizes();
    t::ResourceAllocationSizes<Allocation> bundle_sizes() {
        t::ResourceAllocationSizes<Allocation> result;
        result.set<2, 3>(4);
        return result;
    }

    TEST(TypedDescriptorLayoutsTest, EmptyAndDeclaredEmptyShapesDoNotAllocateHoleSlots) {
        using Empty = t::ResourceSchema<>;
        static_assert(!t::detail::DescriptorLayoutShape<Empty>::NEEDS_EMPTY);
        const t::TypedDescriptorLayouts<Empty> empty(std::array<vsl::graphic_resource::BindingLayout, 0>{});
        EXPECT_FALSE(empty.hole_layout());
        using Sparse = t::ResourceSchema<t::DescriptorSet<500>>;
        using Schema = t::ResourceAllocationSchema<Sparse>;
        constexpr t::TypedBundlePlan<Schema> plan(t::ResourceAllocationPlan<Schema>(2));
        static_assert(!plan.NEEDS_EMPTY_LAYOUT);
        static_assert(plan.empty_descriptor_layout() == plan.descriptor_layout(500));
        static_assert(plan.arena_plan(t::ResourceScope::SHARED).slots() == 3);
        EXPECT_EQ(plan.descriptors().total_sets(), 1u);
        EXPECT_EQ(plan.descriptors().total_pools(), 1u);
    }
    TEST_F(SharedLayoutsVulkanIntegrationTest, CollectionValidatesRealSourcesAndRejectsMissingOrMovedOwners) {
        const auto points = One::Set<0>::POINTS;
        vsl::graphic_resource::BindingLayout source(device, std::vector<vsl::graphic_resource::BindingPoint>(points.begin(), points.end()));
        vsl::graphic_resource::BindingLayout empty(device, std::vector<vsl::graphic_resource::BindingPoint>{});
        EXPECT_THROW((OneLayouts(std::array{source})), std::invalid_argument);
        EXPECT_THROW((OneLayouts(std::array{source}, source)), std::invalid_argument);
        for (const bool borrowed : {false, true}) {
            auto invalid = source;
            invalid.data_ = borrowed ? std::shared_ptr<vsl::impl::ResourceBindingLayoutImpl>(std::shared_ptr<void>{}, source.data_.get()) : nullptr;
            EXPECT_THROW((OneLayouts(std::array{invalid}, empty)), std::invalid_argument);
        }
        for (int field = 0; field < 4; ++field) {
            auto point = points[0];
            if (field == 0)
                ++point.binding;
            else if (field == 1)
                ++point.amount;
            else if (field == 2)
                point.shader_type = vsl::ShaderFlag::VERTEX;
            else
                point.binding_type = vsl::graphic_resource::Type::STORAGE_BUFFER;
            vsl::graphic_resource::BindingLayout incompatible(device, {point});
            EXPECT_THROW((OneLayouts(std::array{incompatible}, empty)), std::invalid_argument);
        }
        OneLayouts layouts(std::array{source}, empty);
        auto moved = std::move(layouts);
        EXPECT_THROW(layouts.layout<2>(), std::invalid_argument);
        EXPECT_THROW((vsl::PipelineLayout(device, t::detail::DescriptorLayoutInjection<One>{layouts})), std::invalid_argument);
        EXPECT_EQ(moved.layout<2>().data_, source.data_);
        EXPECT_EQ(moved.hole_layout()->data_, empty.data_);
    }
    TEST_F(SharedLayoutsVulkanIntegrationTest, BundleSourcesShareHandlesAndFillEveryHoleWithOneLayout) {
        auto manager = make_manager();
        vsl::GraphicResourceManager descriptors(device);
        Bundle bundle(manager, descriptors, bundle_sizes());
        const auto sources = bundle.descriptor_layouts();
        auto phase = manager.start_phase<vsl::ComputePhase>();
        const auto& frame = bundle.get(phase);
        vsl::PipelineLayout layout(device, t::detail::DescriptorLayoutInjection<Resources>{sources});
        ASSERT_EQ(layout.recipe().binding_layouts.size(), Resources::SET_COUNT);
        EXPECT_EQ(layout.recipe().binding_layouts[2], frame.set<2>().backend().data_->layout);
        EXPECT_EQ(layout.recipe().binding_layouts[5], frame.set<5>().backend().data_->layout);
        const auto empty = sources.hole_layout();
        ASSERT_TRUE(empty);
        for (const auto set : {0, 1, 3, 4})
            EXPECT_EQ(layout.recipe().binding_layouts[set], empty->data_.get());
        EXPECT_EQ(layout.recipe().sources.size(), 1u);
        EXPECT_FALSE(sources.layout<2>().data_.owner_before(sources.layout<5>().data_));
        EXPECT_FALSE(empty->data_.owner_before(sources.layout<2>().data_));
        EXPECT_EQ(descriptors.data_->pools.size(), 0u);
        phase.cancel();
    }
    TEST_F(SharedLayoutsVulkanIntegrationTest, SparseDeclaredEmptyLayoutIsReusedAndTheDeviceLimitIsCheckedBeforeExpansion) {
        using Sparse = t::ResourceSchema<t::DescriptorSet<500>>;
        using Schema = t::ResourceAllocationSchema<Sparse>;
        using EmptyBundle = t::TypedResourceBundle<Schema, t::ArenaStorage<>>;
        auto manager = make_manager();
        vsl::GraphicResourceManager descriptors(device);
        EmptyBundle bundle(manager, descriptors);
        const auto sources = bundle.descriptor_layouts();
        EXPECT_EQ(sources.hole_layout()->data_, sources.layout<500>().data_);
        EXPECT_THROW((vsl::PipelineLayout(device, t::detail::DescriptorLayoutInjection<Sparse>{sources})), std::out_of_range);
        using NoResources = t::ResourceAllocationSchema<t::ResourceSchema<>>;
        t::TypedResourceBundle<NoResources, t::ArenaStorage<>> empty(manager, descriptors);
        const auto empty_sources = empty.descriptor_layouts();
        EXPECT_NO_THROW((vsl::PipelineLayout(device, t::detail::DescriptorLayoutInjection<t::ResourceSchema<>>{empty_sources})));
    }
    TEST_F(SharedLayoutsVulkanIntegrationTest, CollectionOutlivesTheBundleWithoutKeepingItsFrameArenas) {
        auto manager = make_manager();
        vsl::GraphicResourceManager descriptors(device);
        std::optional<t::TypedDescriptorLayouts<Resources>> sources;
        std::weak_ptr<void> shared, local;
        {
            Bundle bundle(manager, descriptors, bundle_sizes());
            sources.emplace(bundle.descriptor_layouts());
            shared = sources->layout<2>().data_;
            auto phase = manager.start_phase<vsl::ComputePhase>();
            const auto& frame = bundle.get(phase);
            local = frame.buffer<2, 0>().backend().owner();
            phase.cancel();
        }
        EXPECT_TRUE(local.expired());
        EXPECT_FALSE(shared.expired());
        vsl::Shader<vsl::ShaderType::COMPUTE> shader(device, vsl_test::shader_path("typed_bundle.comp.spv"));
        t::TypedComputePipeline<Resources> pipeline(device, *sources, pl::ShaderGroup("retained sources", {shader}));
        sources.reset();
        EXPECT_TRUE(shared.expired());
        // A fresh bundle has compatible definitions and can use this pipeline after all original sources were reclaimed.
        Bundle next(manager, descriptors, bundle_sizes());
        next.initialize_shared([](Bundle::Shared& values) {
            values.buffer<2, 1>().copy(std::array<std::uint32_t, 1>{10});
            values.buffer<5, 0, 0>().copy(std::array<std::uint32_t, 1>{20});
            values.buffer<5, 0, 1>().copy(std::array<std::uint32_t, 1>{30});
        });
        auto phase = manager.start_phase<vsl::ComputePhase>();
        next.update(phase, [](Bundle::Frame& frame) { frame.buffer<2, 0>().copy(std::array<std::uint32_t, 1>{13}); });
        const auto& frame = next.get(phase);
        phase << frame.bind_resources(pipeline).dispatch(4);
        const vk::MemoryBarrier barrier{.srcAccessMask = vk::AccessFlagBits::eShaderWrite, .dstAccessMask = vk::AccessFlagBits::eHostRead};
        auto buffer = manager.get_buffer();
        buffer.data_->command_buffers[buffer.get_current_buffer_idx()].pipelineBarrier(
            vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eHost, {}, barrier, {}, {});
        ASSERT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
        ASSERT_TRUE(phase.completion().wait());
        std::array<std::uint32_t, 4> actual{};
        frame.buffer<2, 3>().read_into(actual);
        for (size_t i = 0; i < actual.size(); ++i)
            EXPECT_EQ(actual[i], 73u + i);
    }
    TEST_F(SharedLayoutsVulkanIntegrationTest, ShaderAndLayoutsFromOneArenaKeepOneRecipeSourceWithoutRetainingItInThePipeline) {
        auto verify = [&](auto policy) {
            namespace s = vsl::storage;
            namespace l = vsl::lifetime;
            constexpr std::array<t::ResourceRequest, 4> requests{{{{1}, s::ResourceKind::DESCRIPTOR_LAYOUT},
                                                                  {{2}, s::ResourceKind::DESCRIPTOR_LAYOUT},
                                                                  {{3}, s::ResourceKind::DESCRIPTOR_LAYOUT},
                                                                  {{4}, s::ResourceKind::SHADER}}};
            constexpr t::ResourcePlan<4> plan(requests);
            vsl::Shader<vsl::ShaderType::COMPUTE> source(device, vsl_test::shader_path("typed_bundle.comp.spv"));
            const auto words = source.code();
            std::optional<t::TypedComputePipeline<Resources>> pipeline;
            std::weak_ptr<void> owner;
            {
                const std::array<l::ResourceInit, 4> inputs{
                    vsl::DescriptorLayoutCreateInfo{{LocalSet::POINTS.begin(), LocalSet::POINTS.end()}},
                    vsl::DescriptorLayoutCreateInfo{{SharedSet::POINTS.begin(), SharedSet::POINTS.end()}},
                    vsl::DescriptorLayoutCreateInfo{},
                    vsl::ShaderCreateInfo{vsl::ShaderType::COMPUTE, {words.begin(), words.end()}}};
                l::ResourceArena arena(device, plan, inputs);
                owner = arena.owner();
                t::TypedDescriptorLayouts<Resources> layouts(std::array{arena.descriptor_layout({1}), arena.descriptor_layout({2})}, arena.descriptor_layout({3}));
                pl::ShaderGroup shaders("one owner", {arena.shader({4})});
                vsl::PipelineLayout recipe(device, shaders, t::detail::DescriptorLayoutInjection<Resources>{layouts});
                EXPECT_EQ(recipe.recipe().sources.size(), 1u);
                pipeline.emplace(policy, device, layouts, shaders);
            }
            EXPECT_TRUE(owner.expired());
            EXPECT_TRUE(pipeline->backend().data_->pipeline);
        };
        verify(t::HeapStorage{});
        verify(t::ArenaStorage<>{});
    }
} // namespace
