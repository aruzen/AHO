#include <VSL/lifetime/graphics_access.hpp>
#include <VSL/lifetime/shader_access.hpp>
#include "support/render_fixture.hpp"
#include "support/compute_fixture.hpp"
#include <VSL/lifetime/resource_arena.hpp>
#include <VSL/utils/typed_resources.hpp>

namespace {
    using VSL_NAMESPACE::lifetime::detail::GraphicsAccess;
}

namespace {
    using VSL_NAMESPACE::lifetime::detail::ShaderAccess;
}

namespace {
    namespace t = vsl::typed;
    namespace l = vsl::lifetime;
    namespace s = vsl::storage;
    namespace pl = vsl::pipeline_layout;
    constexpr std::array<t::ResourceRequest, 1> PIPELINE_REQUESTS{{{{1}, s::ResourceKind::PIPELINE}}};
    constexpr t::ResourcePlan<1> PIPELINE_PLAN(PIPELINE_REQUESTS);
    constexpr std::array<t::ResourceRequest, 6> MIXED_REQUESTS{{{{1}, s::ResourceKind::PIPELINE}, {{2}, s::ResourceKind::PIPELINE}, {{3}, s::ResourceKind::PIPELINE_LAYOUT}, {{4}, s::ResourceKind::RENDER_PASS}, {{5}, s::ResourceKind::SHADER}, {{6}, s::ResourceKind::SHADER}}};
    constexpr t::ResourcePlan<6> MIXED_PLAN(MIXED_REQUESTS);
    class PipelineArenaVulkanIntegrationTest : public vsl_test::RenderVulkanIntegrationTest {
    protected:
        vsl::GraphicsPipelineState make_state() const;
        l::GraphicsPipelineInit direct_input() const;
        std::array<l::ResourceInit, 6> mixed_inputs() const;
        bool validate_vulkan() const override { return true; }
    };
    class ComputePipelineArenaVulkanIntegrationTest : public vsl_test::ComputeVulkanIntegrationTest {
        bool validate_vulkan() const override { return true; }
    };
    vsl::GraphicsPipelineState PipelineArenaVulkanIntegrationTest::make_state() const {
        return vsl::GraphicsPipelineState(pl::VertexInput{}, pl::ColorBlend{}, pl::InputAssembly{},
                                          pl::Rasterization{pl::CullingMode::NONE}, pl::Multisample{}, pl::DynamicState{}, vsl::Viewport(description), vsl::Scissor(description));
    }
    l::GraphicsPipelineInit PipelineArenaVulkanIntegrationTest::direct_input() const {
        return {layout->runtime(), vsl::RenderPassAccessor{*pass}, make_state(), {vsl::ShaderAccessor{*vertex}, vsl::ShaderAccessor{*fragment}}};
    }
    std::array<l::ResourceInit, 6> PipelineArenaVulkanIntegrationTest::mixed_inputs() const {
        const l::GraphicsPipelineInit input{t::ResourceId{3}, t::ResourceId{4}, make_state(), {t::ResourceId{5}, t::ResourceId{6}}};
        return {input, input, l::PipelineLayoutInit{}, pass->description(),
                vsl::ShaderCreateInfo{vsl::ShaderType::VERTEX, {vertex->code().begin(), vertex->code().end()}},
                vsl::ShaderCreateInfo{vsl::ShaderType::FRAGMENT, {fragment->code().begin(), fragment->code().end()}}};
    }
    TEST(PipelinePlacementTest, ExactSlotsHaveNoSeparateUseStateAndRejectInsufficientRegions) {
        static_assert(s::ResourceSlotLayout<vsl::impl::PipelineImpl>::USE_OFFSET == s::NO_SEPARATE_USE);
        static_assert(MIXED_PLAN.placement({2}).offset == s::PIPELINE_IMPL_SIZE);
        static_assert(PIPELINE_PLAN.bytes() == s::PIPELINE_IMPL_SIZE);
        EXPECT_EQ(PIPELINE_PLAN.alignment(), s::PIPELINE_IMPL_ALIGNMENT);
        EXPECT_THROW(PIPELINE_PLAN.require_region(PIPELINE_PLAN.bytes() - 1, PIPELINE_PLAN.alignment()), std::invalid_argument);
        EXPECT_THROW(PIPELINE_PLAN.require_region(PIPELINE_PLAN.bytes(), PIPELINE_PLAN.alignment() / 2), std::invalid_argument);
        static_assert(!std::is_copy_constructible_v<vsl::impl::PipelineImpl>);
        static_assert(!std::is_move_constructible_v<vsl::impl::PipelineImpl>);
    }
    TEST(PipelinePlacementTest, EmptyFacadesCannotBecomeGraphicsOrComputeCommands) {
        const vsl::PipelineAccessor empty;
        EXPECT_EQ(empty.owner_ref().use_count(), 0);
        EXPECT_THROW(empty.get_device(), std::invalid_argument);
        EXPECT_THROW((vsl::GraphicsPipeline(empty)), std::invalid_argument);
        EXPECT_THROW((vsl::ComputePipeline(empty)), std::invalid_argument);
    }
    TEST_F(PipelineArenaVulkanIntegrationTest, ReverseDependenciesConstructDistinctSlotsAndSavedCommandsKeepOneOwner) {
        std::weak_ptr<void> owner;
        std::optional<vsl::GraphicsPipeline> kept;
        vsl::LogicalDeviceAccessor device_only;
        {
            const auto inputs = mixed_inputs();
            l::ResourceArena arena(device, MIXED_PLAN, inputs);
            owner = arena.owner();
            const auto first = arena.pipeline({1}), second = arena.pipeline({2});
            const auto base = reinterpret_cast<std::uintptr_t>(GraphicsAccess::Owned(first).get());
            EXPECT_EQ(reinterpret_cast<std::uintptr_t>(GraphicsAccess::Owned(second).get()) - base, MIXED_PLAN.placement({2}).offset);
            EXPECT_NE(GraphicsAccess::Owned(first)->pipeline, GraphicsAccess::Owned(second)->pipeline);
            EXPECT_EQ(GraphicsAccess::Owned(first)->layout, GraphicsAccess::Owned(arena.pipeline_layout({3})).get());
            EXPECT_EQ(GraphicsAccess::Owned(first)->layout, GraphicsAccess::Owned(second)->layout);
            EXPECT_FALSE(GraphicsAccess::Owned(first).owner_before(GraphicsAccess::Owned(second)));
            EXPECT_FALSE(GraphicsAccess::Owned(second).owner_before(GraphicsAccess::Owned(first)));
            EXPECT_TRUE(first.owner_ref().managed());
            EXPECT_THROW(arena.pipeline({3}), std::invalid_argument);
            EXPECT_THROW(arena.pipeline({99}), std::out_of_range);
            EXPECT_THROW((vsl::ComputePipeline(first)), std::invalid_argument);
            device_only = first.get_device();
            EXPECT_EQ(device_only.data_, device.data_);
            EXPECT_FALSE(device_only.data_.owner_before(device.data_));
            EXPECT_FALSE(device.data_.owner_before(device_only.data_));
            kept.emplace(first);
        }
        EXPECT_FALSE(owner.expired());
        ExpectTriangleColor(render([&](vsl::DefaultPhaseStream& stream) { stream << *kept << vsl::command::Draw{3}; }));
        kept.reset();
        EXPECT_TRUE(owner.expired());
        EXPECT_EQ(device_only.data_, device.data_);
    }
    TEST_F(PipelineArenaVulkanIntegrationTest, DirectSourcesAreReleasedAfterCreationWhileTheRuntimeLayoutStaysAlive) {
        std::optional<vsl::GraphicsPipeline> kept;
        std::weak_ptr<void> sources, runtime, output_owner;
        {
            constexpr std::array<t::ResourceRequest, 1> requests{{{{1}, s::ResourceKind::PIPELINE_LAYOUT}}};
            constexpr t::ResourcePlan<1> plan(requests);
            const std::array<l::ResourceInit, 1> layout_input{l::PipelineLayoutInit{}};
            l::ResourceArena shared(device, plan, layout_input);
            runtime = shared.owner();
            {
                constexpr std::array<t::ResourceRequest, 3> source_requests{{{{1}, s::ResourceKind::SHADER}, {{2}, s::ResourceKind::SHADER}, {{3}, s::ResourceKind::RENDER_PASS}}};
                constexpr t::ResourcePlan<3> source_plan(source_requests);
                const std::array<l::ResourceInit, 3> source_inputs{vsl::ShaderCreateInfo{vsl::ShaderType::VERTEX, {vertex->code().begin(), vertex->code().end()}},
                                                                   vsl::ShaderCreateInfo{vsl::ShaderType::FRAGMENT, {fragment->code().begin(), fragment->code().end()}}, pass->description()};
                l::ResourceArena source(device, source_plan, source_inputs);
                sources = source.owner();
                const std::array<l::ResourceInit, 1> inputs{l::GraphicsPipelineInit{shared.pipeline_layout({1}), source.render_pass({3}), make_state(), {source.shader({1}), source.shader({2})}}};
                l::ResourceArena arena(device, PIPELINE_PLAN, inputs);
                output_owner = arena.owner();
                kept.emplace(arena.pipeline({1}));
            }
            EXPECT_TRUE(sources.expired());
        }
        EXPECT_FALSE(runtime.expired());
        ExpectTriangleColor(render([&](vsl::DefaultPhaseStream& stream) { stream << *kept << vsl::command::Draw{3}; }));
        kept.reset();
        EXPECT_TRUE(output_owner.expired());
        EXPECT_TRUE(runtime.expired());
    }
    TEST_F(PipelineArenaVulkanIntegrationTest, LaterFailureDestroysEarlierPipelinesAndAllowsTheSameRegionToBeReused) {
        s::ArenaMemory memory(MIXED_PLAN.bytes(), MIXED_PLAN.alignment());
        auto inputs = mixed_inputs();
        auto& invalid = std::get<l::GraphicsPipelineInit>(inputs[1]);
        pl::ColorBlend{}.injection(invalid.state.edit());
        for (int attempt = 0; attempt < 3; ++attempt)
            EXPECT_THROW((l::ResourceArena(device, MIXED_PLAN, inputs, memory)), std::invalid_argument);
        inputs[1] = inputs[0];
        EXPECT_NO_THROW((l::ResourceArena(device, MIXED_PLAN, inputs, memory)));
        std::get<l::GraphicsPipelineInit>(inputs[1]).subpass = 99;
        EXPECT_THROW((l::ResourceArena(device, MIXED_PLAN, inputs, memory)), std::out_of_range);
        inputs[1] = inputs[0];
        EXPECT_NO_THROW((l::ResourceArena(device, MIXED_PLAN, inputs, memory)));
    }
    TEST_F(PipelineArenaVulkanIntegrationTest, HeapLayoutEdgesAndPipelineImportsRejectAReverseDescriptorDependency) {
        using Set = t::DescriptorSet<0, t::UniformBinding<0, std::uint32_t, vsl::ShaderFlag::FRAGMENT>>;
        using View = t::TypedResourceBufferView<std::uint32_t, vsl::graphic_resource::Type::UNIFORM_BUFFER>;
        constexpr std::array<t::ResourceRequest, 5> requests{{{{1}, s::ResourceKind::PIPELINE_LAYOUT}, {{2}, s::ResourceKind::BUFFER}, {{3}, s::ResourceKind::DESCRIPTOR_LAYOUT}, {{4}, s::ResourceKind::DESCRIPTOR_POOL}, {{5}, s::ResourceKind::DESCRIPTOR_SET}}};
        constexpr t::ResourcePlan<5> plan(requests);
        const vsl::BufferCreateInfo buffer{4, vsl::MemoryType::UNIFORM_BUFFER};
        const vsl::DescriptorLayoutCreateInfo binding{{Set::POINTS.begin(), Set::POINTS.end()}};
        const vsl::DescriptorPoolCreateInfo pool{{{vsl::graphic_resource::Type::UNIFORM_BUFFER, 1}}, 1};
        const std::array<l::ResourceInit, 5> inputs{l::PipelineLayoutInit{{t::ResourceId{3}}, {}}, buffer, binding, pool, l::DescriptorSetInit{{4}, {3}}};
        std::weak_ptr<void> source_owner, heap_owner, importer;
        {
            l::ResourceArena source(device, plan, inputs, l::DependencyPolicy::DYNAMIC);
            source_owner = source.owner();
            const std::array<vsl::ShaderAccessor, 2> shaders{*vertex, *fragment};
            vsl::GraphicsPipeline selected(source.pipeline_layout({1}), *pass, make_state(), shaders);
            heap_owner = GraphicsAccess::Owned(selected).share();
            auto set = t::TypedDescriptorSet<Set>::FromResource(device, source.descriptor_set({5}));
            set.write<0>(View::FromView(source.buffer({2}), 1));
            EXPECT_THROW(set.write<0>(View::FromView(source.buffer({2}), 1, 0, {{}, selected.owner_ref()})), std::invalid_argument);
            vsl::PipelineAccessor kept;
            {
                constexpr std::array<t::ResourceRequest, 2> imports{{{{1}, s::ResourceKind::PIPELINE, true}, {{2}, s::ResourceKind::BUFFER}}};
                constexpr t::ResourcePlan<2> import_plan(imports);
                const std::array<l::ResourceInit, 2> imported{vsl::PipelineAccessor{selected}, buffer};
                l::ResourceArena target(device, import_plan, imported, l::DependencyPolicy::DYNAMIC);
                importer = target.owner();
                kept = target.pipeline({1});
                EXPECT_EQ(GraphicsAccess::Owned(kept), GraphicsAccess::Owned(selected));
                EXPECT_FALSE(GraphicsAccess::Owned(kept).owner_before(GraphicsAccess::Owned(selected)));
                EXPECT_FALSE(GraphicsAccess::Owned(selected).owner_before(GraphicsAccess::Owned(kept)));
                EXPECT_TRUE(kept.owner_ref().managed());
                EXPECT_THROW(set.write<0>(View::FromView(target.buffer({2}), 1)), std::invalid_argument);
            }
            EXPECT_TRUE(importer.expired());
            EXPECT_NO_THROW(set.require_complete());
        }
        EXPECT_TRUE(source_owner.expired());
        EXPECT_TRUE(heap_owner.expired());
    }
    TEST_F(PipelineArenaVulkanIntegrationTest, GraphicsSourcesRejectEmptyAndNonowningShaderAccessors) {
        const auto borrowed = ShaderAccess::FromShared(std::shared_ptr<vsl::impl::ShaderImpl>(std::shared_ptr<void>{}, ShaderAccess::Owned(*(vertex)).get()));
        const auto missing_module = ShaderAccess::FromShared(std::make_shared<vsl::impl::ShaderImpl>());
        ShaderAccess::Owned(missing_module)->device = device.data_.get();
        for (const auto& invalid : {vsl::ShaderAccessor{}, borrowed, missing_module}) {
            auto input = direct_input();
            input.shaders[0] = invalid;
            const std::array<l::ResourceInit, 1> inputs{input};
            EXPECT_THROW((l::ResourceArena(device, PIPELINE_PLAN, inputs)), std::invalid_argument);
            const std::array<vsl::ShaderAccessor, 2> shaders{invalid, *fragment};
            EXPECT_THROW((vsl::GraphicsPipeline(layout->runtime(), *pass, input.state, shaders)), std::invalid_argument);
        }
    }
    TEST_F(PipelineArenaVulkanIntegrationTest, CompletionAndCancelKeepRecordedPipelinesUntilWorkOwnersReleaseThem) {
        auto manager = make_manager();
        for (const bool submit : {false, true}) {
            std::weak_ptr<void> owner;
            vsl::Completion completion;
            {
                auto phase = manager.start_phase<vsl::ComputePhase>();
                completion = phase.completion();
                {
                    const std::array<l::ResourceInit, 1> inputs{direct_input()};
                    l::ResourceArena arena(device, PIPELINE_PLAN, inputs);
                    owner = arena.owner();
                    vsl::GraphicsPipeline selected(arena.pipeline({1}));
                    phase << vsl::command::RenderPassBegin(*framebuffer) << selected << vsl::Viewport(description)
                          << vsl::Scissor(description) << vsl::command::Draw{3} << vsl::command::RenderPassEnd{};
                }
                EXPECT_FALSE(owner.expired());
                if (submit) {
                    ASSERT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
                    ASSERT_TRUE(completion.wait());
                } else {
                    phase.cancel();
                }
                EXPECT_FALSE(owner.expired());
            }
            if (submit) {
                EXPECT_FALSE(owner.expired());
                auto recycle = manager.start_phase<vsl::ComputePhase>();
                ASSERT_EQ(recycle.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
                ASSERT_TRUE(recycle.completion().wait());
            }
            EXPECT_TRUE(owner.expired());
            EXPECT_EQ(completion.status(), submit ? vsl::Completion::Status::COMPLETE : vsl::Completion::Status::CANCELLED);
        }
    }
    TEST_F(ComputePipelineArenaVulkanIntegrationTest, DirectComputeSourceCanBeReleasedBeforeDispatchAndPushRecording) {
        std::weak_ptr<void> source, owner;
        {
            vsl::Shader<vsl::ShaderType::COMPUTE> local(device, shader->code());
            source = ShaderAccess::Owned(local).share();
            const std::array<l::ResourceInit, 1> inputs{l::ComputePipelineInit{layout->runtime(), vsl::ShaderAccessor{local}}};
            l::ResourceArena arena(device, PIPELINE_PLAN, inputs);
            owner = arena.owner();
            EXPECT_THROW((vsl::GraphicsPipeline(arena.pipeline({1}))), std::invalid_argument);
            pipeline.emplace(arena.pipeline({1}));
        }
        EXPECT_TRUE(source.expired());
        EXPECT_FALSE(owner.expired());
        ExpectSequence(execute(379), 379);
        pipeline.reset();
        EXPECT_TRUE(owner.expired());
    }
    TEST_F(ComputePipelineArenaVulkanIntegrationTest, ComputeSourcesRejectNonownersAndWrongShaderStages) {
        const auto borrowed = ShaderAccess::FromShared(std::shared_ptr<vsl::impl::ShaderImpl>(std::shared_ptr<void>{}, ShaderAccess::Owned(*(shader)).get()));
        const auto missing_module = ShaderAccess::FromShared(std::make_shared<vsl::impl::ShaderImpl>());
        ShaderAccess::Owned(missing_module)->device = device.data_.get();
        for (const auto& invalid : {vsl::ShaderAccessor{}, borrowed, missing_module}) {
            const std::array<l::ResourceInit, 1> inputs{l::ComputePipelineInit{layout->runtime(), invalid}};
            EXPECT_THROW((l::ResourceArena(device, PIPELINE_PLAN, inputs)), std::invalid_argument);
            EXPECT_THROW((vsl::ComputePipeline(layout->runtime(), invalid)), std::invalid_argument);
        }
        vsl::Shader<vsl::ShaderType::VERTEX> vertex_shader(device, vsl_test::shader_path("triangle.vert.spv"));
        const std::array<l::ResourceInit, 1> inputs{l::ComputePipelineInit{layout->runtime(), vsl::ShaderAccessor{vertex_shader}}};
        EXPECT_THROW((l::ResourceArena(device, PIPELINE_PLAN, inputs)), std::invalid_argument);
        EXPECT_THROW((vsl::ComputePipeline(layout->runtime(), vertex_shader)), std::invalid_argument);
    }
} // namespace

namespace {
    TEST_F(PipelineArenaVulkanIntegrationTest, RetiredOwnersCannotPublishShaderOrGraphicsOwnership) {
        const auto initializers = mixed_inputs();
        l::ResourceArena arena(device, MIXED_PLAN, initializers);
        const auto pipeline = arena.pipeline({1});
        const auto layout = arena.pipeline_layout({3});
        const auto pass = arena.render_pass({4});
        const auto shader = arena.shader({5});
        const auto native_pipeline = GraphicsAccess::Owned(pipeline)->pipeline;
        const auto native_layout = GraphicsAccess::Owned(layout)->pipeline_layout;
        const auto native_pass = GraphicsAccess::Owned(pass)->render_pass;
        const auto native_shader = ShaderAccess::Owned(shader)->shader_module;
        const_cast<l::DependencyNode*>(GraphicsAccess::Owned(pipeline)->owner_node)->retire();
        EXPECT_THROW(pipeline.owner_ref(), std::invalid_argument);
        EXPECT_THROW(layout.owner_ref(), std::invalid_argument);
        EXPECT_THROW(pass.owner_ref(), std::invalid_argument);
        EXPECT_THROW(shader.owner_ref(), std::invalid_argument);
        EXPECT_EQ(GraphicsAccess::Owned(pipeline)->pipeline, native_pipeline);
        EXPECT_EQ(GraphicsAccess::Owned(layout)->pipeline_layout, native_layout);
        EXPECT_EQ(GraphicsAccess::Owned(pass)->render_pass, native_pass);
        EXPECT_EQ(ShaderAccess::Owned(shader)->shader_module, native_shader);
    }
} // namespace
