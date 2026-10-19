#include "support/render_fixture.hpp"
namespace {
    using GraphicsPipelineVulkanIntegrationTest = vsl_test::RenderVulkanIntegrationTest;
    TEST_F(GraphicsPipelineVulkanIntegrationTest, CreatesPipelineAndInvokesItToProducePixels) {
        ASSERT_TRUE(static_cast<bool>(pipeline->_data->pipeline));
        EXPECT_EQ(pipeline->_data->layout, layout->_data);
        expect_triangle_color(render([](auto &stream) { stream << vsl::command::Draw{3}; }));
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
