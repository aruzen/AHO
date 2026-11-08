#include <VSL/operation/graphics_pipeline_state.hpp>
#include <VSL/utils/typed_pipeline.hpp>
#include <VSL/vulkan/stages/id_picking.hpp>
#include "support/render_fixture.hpp"

namespace {
    namespace pl = vsl::pipeline_layout;
    namespace t = vsl::typed;
    struct Position {
        float xy[2];
    };
    using Input = t::TypedVertexInput<t::VertexRecord<Position, VSL_VERTEX_MEMBER(Position, xy)>::Binding<4>>;
    template<typename T>
    concept temporary_state_definition = requires(T&& value) { std::move(value).definition(); };
    template<typename T>
    concept temporary_state_edit = requires(T&& value) { std::move(value).edit(); };
    static_assert(!temporary_state_definition<vsl::GraphicsPipelineState>);
    static_assert(!temporary_state_definition<const vsl::GraphicsPipelineState>);
    static_assert(!temporary_state_edit<vsl::GraphicsPipelineState>);
    static_assert(!vsl::graphics_pipeline_state_injecter<pl::ShaderGroup>);
    static_assert(!vsl::graphics_pipeline_state_injecter<pl::ResourceBinding>);
    static_assert(!vsl::graphics_pipeline_state_injecter<pl::PushConstants>);
    TEST(GraphicsPipelineStateTest, DeviceIndependentDefinitionKeepsResolvedTypedMetadataAndOrderedStages) {
        vsl::GraphicsPipelineState state(Input{}, pl::InputAssembly{}, pl::Rasterization{pl::CullingMode::NONE},
                                         pl::Multisample{}, pl::ColorBlend{}, pl::IDPicking{}, pl::DepthStencil{}, pl::DynamicState{},
                                         vsl::Viewport(1, 2, 10, 20), vsl::Scissor(3, 4, 8, 16));
        const auto& data = state.definition();
        ASSERT_EQ(data.vertex_bindings.size(), 1u);
        EXPECT_EQ(data.vertex_bindings[0].binding, 4u);
        EXPECT_EQ(data.vertex_bindings[0].stride, sizeof(Position));
        ASSERT_EQ(data.vertex_attributes.size(), 1u);
        EXPECT_EQ(data.vertex_attributes[0].location, 0u);
        EXPECT_EQ(data.vertex_attributes[0].binding, 4u);
        EXPECT_EQ(data.vertex_attributes[0].format, vk::Format::eR32G32Sfloat);
        ASSERT_EQ(data.blend_attachments.size(), 2u);
        EXPECT_FALSE(data.blend_attachments[1].blendEnable);
        EXPECT_EQ(data.blend_attachments[1].colorWriteMask, vk::ColorComponentFlagBits::eR);
        EXPECT_EQ(data.input_assembly.topology, vk::PrimitiveTopology::eTriangleList);
        EXPECT_EQ(data.rasterization.cullMode, vk::CullModeFlagBits::eNone);
        EXPECT_EQ(data.multisample.rasterizationSamples, vk::SampleCountFlagBits::e1);
        ASSERT_TRUE(data.depth_stencil);
        EXPECT_TRUE(data.depth_stencil->depthTestEnable);
        const vsl::operation::PipelineScratch scratch(data);
        EXPECT_EQ(scratch.vertex_input.pVertexBindingDescriptions, data.vertex_bindings.data());
        EXPECT_EQ(scratch.vertex_input.pVertexAttributeDescriptions, data.vertex_attributes.data());
        EXPECT_EQ(scratch.color_blend.pAttachments, data.blend_attachments.data());
        EXPECT_EQ(scratch.dynamic_state.pDynamicStates, data.enabled_dynamic_states.data());
        EXPECT_EQ(scratch.viewport.pViewports, data.viewports.data());
        EXPECT_EQ(scratch.viewport.pScissors, data.scissors.data());
        EXPECT_EQ(scratch.viewport.viewportCount, 1u);
        EXPECT_EQ(scratch.viewport.scissorCount, 1u);
        EXPECT_FLOAT_EQ(scratch.viewport.pViewports[0].width, 10.f);
        EXPECT_EQ(scratch.viewport.pScissors[0].extent, (vk::Extent2D{8, 16}));
    }
    TEST(GraphicsPipelineStateTest, DefaultInputDoesNotImplicitlyInjectAnyStage) {
        const vsl::GraphicsPipelineState state;
        const auto& data = state.definition();
        EXPECT_TRUE(data.vertex_bindings.empty());
        EXPECT_TRUE(data.vertex_attributes.empty());
        EXPECT_TRUE(data.blend_attachments.empty());
        EXPECT_TRUE(data.viewports.empty());
        EXPECT_TRUE(data.scissors.empty());
        EXPECT_TRUE(data.enabled_dynamic_states.empty());
        EXPECT_FALSE(data.depth_stencil);
        EXPECT_THROW((vsl::GraphicsPipelineStateEditRef{}.get()), std::logic_error);
    }
    TEST(GraphicsPipelineStateTest, CopyAndAssignmentAreDeepAndMovesConsumeOnlyTheSource) {
        vsl::GraphicsPipelineState original(Input{}, pl::ColorBlend{}, vsl::Viewport{});
        vsl::GraphicsPipelineState copied(original), assigned;
        assigned = original;
        EXPECT_NE(&copied.definition(), &original.definition());
        EXPECT_NE(copied.definition().vertex_bindings.data(), original.definition().vertex_bindings.data());
        EXPECT_NE(assigned.definition().blend_attachments.data(), original.definition().blend_attachments.data());
        pl::IDPicking{}.injection(copied.edit());
        vsl::Viewport(0, 0, 2, 2).injection(assigned.edit());
        EXPECT_EQ(original.definition().blend_attachments.size(), 1u);
        EXPECT_EQ(copied.definition().blend_attachments.size(), 2u);
        EXPECT_EQ(original.definition().viewports.size(), 1u);
        EXPECT_EQ(assigned.definition().viewports.size(), 2u);
        const auto* identity = &copied.definition();
        vsl::GraphicsPipelineState moved(std::move(copied));
        EXPECT_EQ(&moved.definition(), identity);
        EXPECT_THROW(copied.definition(), std::logic_error);
        EXPECT_THROW(copied.edit(), std::logic_error);
        assigned = std::move(moved);
        EXPECT_EQ(&assigned.definition(), identity);
        EXPECT_THROW(moved.definition(), std::logic_error);
        copied = original;
        EXPECT_EQ(copied.definition().blend_attachments.size(), 1u);
    }
    TEST(GraphicsPipelineStateTest, RejectedVertexInjectionPreservesThePreviousDefinitionWithoutADevice) {
        vsl::GraphicsPipelineState state(Input{});
        const auto* bindings = state.definition().vertex_bindings.data();
        const auto* attributes = state.definition().vertex_attributes.data();
        pl::VertexInput invalid({{.layouts = {{vsl::data_format::FLOAT_RG, 0, UINT32_MAX - 2}}}});
        EXPECT_THROW(invalid.injection(state.edit()), vsl::exceptions::RuntimeException);
        EXPECT_EQ(state.definition().vertex_bindings.data(), bindings);
        EXPECT_EQ(state.definition().vertex_attributes.data(), attributes);
        EXPECT_EQ(state.definition().vertex_bindings[0].binding, 4u);
    }
    class GraphicsPipelineStateVulkanIntegrationTest : public vsl_test::RenderVulkanIntegrationTest {
        bool validate_vulkan() const override { return true; }
    };
    TEST_F(GraphicsPipelineStateVulkanIntegrationTest, SavedStateBuildsATypedPipelineAndInputIsInjectedLast) {
        std::optional<t::TypedGraphicsPipeline<t::TypedVertexInput<>>> selected;
        {
            vsl::GraphicsPipelineState state(Input{}, pl::InputAssembly{}, pl::Rasterization{pl::CullingMode::NONE},
                                             pl::Multisample{}, pl::ColorBlend{}, pl::DynamicState{}, vsl::Viewport(description), vsl::Scissor(description));
            selected.emplace(device, *pass, pl::ShaderGroup("state input", {*vertex, *fragment}), state);
            pl::IDPicking{}.injection(state.edit());
        }
        ExpectTriangleColor(render([&](vsl::DefaultPhaseStream& stream) { stream << selected->bind_vertices().draw(3); }));
    }
    TEST_F(GraphicsPipelineStateVulkanIntegrationTest, SnapshotInjectionReplacesFixedStateWithoutReplacingLayoutSources) {
        vsl::GraphicsPipelineState state(pl::InputAssembly{}, pl::Rasterization{pl::CullingMode::FRONT});
        const auto runtime = layout->data_;
        const auto shaders = layout->recipe().shaders;
        const auto owner_count = layout->recipe().sources.size();
        state.injection(*layout);
        EXPECT_EQ(layout->data_, runtime);
        EXPECT_EQ(layout->recipe().shaders, shaders);
        EXPECT_EQ(layout->recipe().sources.size(), owner_count);
        EXPECT_EQ(layout->recipe().graphics_state.rasterization.cullMode, vk::CullModeFlagBits::eFront);
        EXPECT_TRUE(layout->recipe().graphics_state.blend_attachments.empty());
        pl::Rasterization{pl::CullingMode::BACK}.injection(state.edit());
        EXPECT_EQ(layout->recipe().graphics_state.rasterization.cullMode, vk::CullModeFlagBits::eFront);
        vsl::GraphicsPipelineState moved(std::move(state));
        EXPECT_THROW(state.injection(*layout), std::logic_error);
        EXPECT_EQ(layout->recipe().graphics_state.rasterization.cullMode, vk::CullModeFlagBits::eFront);
    }
} // namespace
