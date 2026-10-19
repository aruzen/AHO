#include "support/render_fixture.hpp"
namespace {
    using GraphicsPipelineVulkanIntegrationTest = vsl_test::RenderVulkanIntegrationTest;
    TEST_F(GraphicsPipelineVulkanIntegrationTest, CreatesPipelineAndInvokesItToProducePixels) {
        ASSERT_TRUE(static_cast<bool>(pipeline->_data->pipeline));
        EXPECT_EQ(pipeline->_data->layout, layout->_data);
        expect_triangle_color(render([](auto& stream) { stream << vsl::command::Draw{3}; }));
    }
} // namespace
#include "support/compute_fixture.hpp"
namespace {
    using ComputePipelineVulkanIntegrationTest = vsl_test::ComputeVulkanIntegrationTest;
    TEST_F(ComputePipelineVulkanIntegrationTest, CreatesAndInvokesComputePipeline) {
        ASSERT_TRUE(static_cast<bool>(pipeline->_data->pipeline));
        EXPECT_EQ(pipeline->_data->layout, layout->_data);
        expect_sequence(execute(211), 211);
    }
} // namespace

namespace {
    class PipelineSnapshotVulkanIntegrationTest : public vsl_test::RenderVulkanIntegrationTest {
        bool validate_vulkan() const override { return true; }
    };
    TEST_F(PipelineSnapshotVulkanIntegrationTest, FrozenCopyBuildsAfterOriginalShadersAndLayoutAreReleased) {
        auto frozen = layout->copy().freeze();
        std::weak_ptr<vsl::_impl::Shader_impl> weak_vertex = vertex->_data;
        vertex.reset();
        fragment.reset();
        layout.reset();
        EXPECT_FALSE(weak_vertex.expired());
        pipeline.emplace(frozen, *pass);
        expect_triangle_color(render([](auto& stream) { stream << vsl::command::Draw{3}; }));
    }
    TEST_F(PipelineSnapshotVulkanIntegrationTest, EditingLayoutKeepsExistingPipelineSnapshotAlive) {
        const auto snapshot = pipeline->_data->layout;
        layout->add(vsl::pipeline_layout::Rasterization(vsl::pipeline_layout::CullingMode::Front));
        EXPECT_NE(layout->_data, snapshot);
        EXPECT_EQ(pipeline->_data->layout, snapshot);
        expect_triangle_color(render([](auto& stream) { stream << vsl::command::Draw{3}; }));
    }
    TEST_F(ComputePipelineVulkanIntegrationTest, FrozenLayoutBuildsComputePipelineAndEmptyStagesAreRejected) {
        auto frozen = layout->copy().freeze();
        pipeline.emplace(frozen);
        expect_sequence(execute(223), 223);
        vsl::PipelineLayout empty(device);
        EXPECT_THROW(vsl::ComputePipeline{empty}, std::invalid_argument);
        EXPECT_THROW(vsl::ComputePipeline{vsl::PipelineLayoutAccessor{}}, std::invalid_argument);
    }
} // namespace
