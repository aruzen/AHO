#include <VSL/lifetime/graphics_access.hpp>
#include <VSL/lifetime/shader_access.hpp>
#include "support/render_fixture.hpp"
#include <VSL/operation/pipeline_ops.hpp>
#include <functional>

namespace {
    using VSL_NAMESPACE::lifetime::detail::GraphicsAccess;
}

namespace {
    using VSL_NAMESPACE::lifetime::detail::ShaderAccess;
}
namespace {
    struct GraphicsCreateAction {
        std::function<void(const vk::GraphicsPipelineCreateInfo&)> action;
        static vk::Result Create(void* context, vk::Device device, vk::PipelineCache cache,
                                 const vk::GraphicsPipelineCreateInfo& info, vk::Pipeline* output);
    };
    using GraphicsPipelineVulkanIntegrationTest = vsl_test::RenderVulkanIntegrationTest;
    TEST_F(GraphicsPipelineVulkanIntegrationTest, CreatesPipelineAndInvokesItToProducePixels) {
        ASSERT_TRUE(static_cast<bool>(GraphicsAccess::Owned(*(pipeline))->pipeline));
        EXPECT_EQ(GraphicsAccess::Owned(*(pipeline))->layout, GraphicsAccess::Owned(*(layout)).get());
        ExpectTriangleColor(render([](auto& stream) { stream << vsl::command::Draw{3}; }));
    }
} // namespace
#include "support/compute_fixture.hpp"
namespace {
    using ComputePipelineVulkanIntegrationTest = vsl_test::ComputeVulkanIntegrationTest;
    TEST_F(ComputePipelineVulkanIntegrationTest, CreatesAndInvokesComputePipeline) {
        ASSERT_TRUE(static_cast<bool>(GraphicsAccess::Owned(*(pipeline))->pipeline));
        EXPECT_EQ(GraphicsAccess::Owned(*(pipeline))->layout, GraphicsAccess::Owned(*(layout)).get());
        ExpectSequence(execute(211), 211);
    }
} // namespace

namespace {
    class PipelineSnapshotVulkanIntegrationTest : public vsl_test::RenderVulkanIntegrationTest {
        bool validate_vulkan() const override { return true; }
    };
    vk::Result GraphicsCreateAction::Create(void* context, vk::Device device, vk::PipelineCache cache,
                                            const vk::GraphicsPipelineCreateInfo& info, vk::Pipeline* output) {
        const auto action = static_cast<GraphicsCreateAction*>(context)->action;
        action(info);
        return device.createGraphicsPipelines(cache, 1, &info, nullptr, output);
    }
    TEST_F(PipelineSnapshotVulkanIntegrationTest, ExplicitCreationKeepsItsShaderOwnersAndStateUntilNativeCreationFinishes) {
        namespace pl = vsl::pipeline_layout;
        vsl::GraphicsPipelineState state;
        state.edit().get() = layout->recipe().graphics_state;
        pl::Rasterization{pl::CullingMode::NONE}.injection(state.edit());
        const auto runtime = layout->runtime();
        std::vector<vsl::ShaderAccessor> sources{
            vsl::Shader<vsl::ShaderType::VERTEX>(device, vertex->code()),
            vsl::Shader<vsl::ShaderType::FRAGMENT>(device, fragment->code())};
        const std::weak_ptr<void> first = ShaderAccess::Owned(sources[0]).share();
        const std::weak_ptr<void> second = ShaderAccess::Owned(sources[1]).share();
        bool called = false;
        GraphicsCreateAction action{[&](const vk::GraphicsPipelineCreateInfo& info) {
            called = true;
            EXPECT_NE(info.pRasterizationState, &state.definition().rasterization);
            pl::Rasterization{pl::CullingMode::FRONT_AND_BACK}.injection(state.edit());
            sources.clear();
            EXPECT_FALSE(first.expired());
            EXPECT_FALSE(second.expired());
            if (first.expired() || second.expired())
                throw std::logic_error("Native inputs lost their shader owners");
        }};
        {
            const vsl::operation::ScopedGraphicsPipelineNativeFactory factory({&action, GraphicsCreateAction::Create});
            pipeline.emplace(runtime, *pass, state, sources);
        }
        EXPECT_TRUE(called);
        EXPECT_TRUE(first.expired());
        EXPECT_TRUE(second.expired());
        EXPECT_EQ(state.definition().rasterization.cullMode, vk::CullModeFlags(vk::CullModeFlagBits::eFrontAndBack));
        ExpectTriangleColor(render([](auto& stream) { stream << vsl::command::Draw{3}; }));
    }
    TEST_F(PipelineSnapshotVulkanIntegrationTest, RecipeCreationKeepsItsSelectedStateWhenASavedEditorChangesTheRecipe) {
        namespace pl = vsl::pipeline_layout;
        layout->add(pl::Rasterization{pl::CullingMode::NONE});
        vsl::PipelineLayoutEditAccessor editor = *layout;
        bool called = false;
        GraphicsCreateAction action{[&](const vk::GraphicsPipelineCreateInfo& info) {
            called = true;
            EXPECT_NE(info.pRasterizationState, &editor.recipe().graphics_state.rasterization);
            pl::Rasterization{pl::CullingMode::FRONT_AND_BACK}.injection(editor);
        }};
        {
            const vsl::operation::ScopedGraphicsPipelineNativeFactory factory({&action, GraphicsCreateAction::Create});
            pipeline.emplace(*layout, *pass);
        }
        EXPECT_TRUE(called);
        EXPECT_EQ(layout->recipe().graphics_state.rasterization.cullMode, vk::CullModeFlags(vk::CullModeFlagBits::eFrontAndBack));
        ExpectTriangleColor(render([](auto& stream) { stream << vsl::command::Draw{3}; }));
    }
    TEST_F(PipelineSnapshotVulkanIntegrationTest, ShaderOwnershipIsValidatedBeforeAMovedStateIsRead) {
        vsl::GraphicsPipelineState moved;
        const auto kept = std::move(moved);
        const auto runtime = layout->runtime();
        const std::array<vsl::ShaderAccessor, 1> invalid{};
        EXPECT_THROW((vsl::GraphicsPipeline(runtime, *pass, moved, invalid)), std::invalid_argument);
        const std::array<vsl::ShaderAccessor, 2> valid{*vertex, *fragment};
        EXPECT_THROW((vsl::GraphicsPipeline(runtime, *pass, moved, valid)), std::logic_error);
    }
    TEST_F(PipelineSnapshotVulkanIntegrationTest, FrozenCopyBuildsAfterOriginalShadersAndLayoutAreReleased) {
        auto frozen = layout->copy().freeze();
        std::weak_ptr<vsl::impl::ShaderImpl> weak_vertex = ShaderAccess::Owned(*(vertex)).share();
        vertex.reset();
        fragment.reset();
        layout.reset();
        EXPECT_FALSE(weak_vertex.expired());
        pipeline.emplace(frozen, *pass);
        ExpectTriangleColor(render([](auto& stream) { stream << vsl::command::Draw{3}; }));
    }
    TEST_F(PipelineSnapshotVulkanIntegrationTest, EditingLayoutKeepsExistingPipelineSnapshotAlive) {
        const auto snapshot = GraphicsAccess::Owned(*(pipeline))->layout;
        layout->add(vsl::pipeline_layout::Rasterization(vsl::pipeline_layout::CullingMode::FRONT));
        EXPECT_NE(GraphicsAccess::Owned(*(layout)).get(), snapshot);
        EXPECT_EQ(GraphicsAccess::Owned(*(pipeline))->layout, snapshot);
        ExpectTriangleColor(render([](auto& stream) { stream << vsl::command::Draw{3}; }));
    }
    TEST_F(ComputePipelineVulkanIntegrationTest, FrozenLayoutBuildsComputePipelineAndEmptyStagesAreRejected) {
        auto frozen = layout->copy().freeze();
        pipeline.emplace(frozen);
        ExpectSequence(execute(223), 223);
        vsl::PipelineLayout empty(device);
        EXPECT_THROW(vsl::ComputePipeline{empty}, std::invalid_argument);
        EXPECT_THROW(vsl::ComputePipeline{vsl::PipelineLayoutAccessor{}}, std::invalid_argument);
    }
} // namespace
