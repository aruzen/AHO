#include "support/render_fixture.hpp"
namespace {
    using GraphicsPipelineVulkanIntegrationTest = vsl_test::RenderVulkanIntegrationTest;
    TEST_F(GraphicsPipelineVulkanIntegrationTest, CreatesPipelineAndInvokesItToProducePixels) {
        ASSERT_TRUE(static_cast<bool>(pipeline->data_->pipeline));
        EXPECT_EQ(pipeline->data_->layout, layout->data_);
        ExpectTriangleColor(render([](auto& stream) { stream << vsl::command::Draw{3}; }));
    }
} // namespace
#include "support/compute_fixture.hpp"
namespace {
    using ComputePipelineVulkanIntegrationTest = vsl_test::ComputeVulkanIntegrationTest;
    TEST_F(ComputePipelineVulkanIntegrationTest, CreatesAndInvokesComputePipeline) {
        ASSERT_TRUE(static_cast<bool>(pipeline->data_->pipeline));
        EXPECT_EQ(pipeline->data_->layout, layout->data_);
        ExpectSequence(execute(211), 211);
    }
} // namespace

namespace {
    class PipelineSnapshotVulkanIntegrationTest : public vsl_test::RenderVulkanIntegrationTest {
        bool validate_vulkan() const override { return true; }
    };
    TEST_F(PipelineSnapshotVulkanIntegrationTest, FrozenCopyBuildsAfterOriginalShadersAndLayoutAreReleased) {
        auto frozen = layout->copy().freeze();
        std::weak_ptr<vsl::impl::ShaderImpl> weak_vertex = vertex->data_;
        vertex.reset();
        fragment.reset();
        layout.reset();
        EXPECT_FALSE(weak_vertex.expired());
        pipeline.emplace(frozen, *pass);
        ExpectTriangleColor(render([](auto& stream) { stream << vsl::command::Draw{3}; }));
    }
    TEST_F(PipelineSnapshotVulkanIntegrationTest, EditingLayoutKeepsExistingPipelineSnapshotAlive) {
        const auto snapshot = pipeline->data_->layout;
        layout->add(vsl::pipeline_layout::Rasterization(vsl::pipeline_layout::CullingMode::FRONT));
        EXPECT_NE(layout->data_, snapshot);
        EXPECT_EQ(pipeline->data_->layout, snapshot);
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
