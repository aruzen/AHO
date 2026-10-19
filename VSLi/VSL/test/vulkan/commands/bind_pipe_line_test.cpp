#include "support/render_fixture.hpp"
namespace {
    using BindPipelineVulkanIntegrationTest = vsl_test::RenderVulkanIntegrationTest;
    TEST_F(BindPipelineVulkanIntegrationTest, RetainsAndBindsGraphicsPipeline) {
        vsl::command::BindPipeline binding(*pipeline);
        EXPECT_EQ(binding.pipeline._data, pipeline->_data);
        expect_triangle_color(render([&](auto &stream) { stream << binding << vsl::command::Draw{3}; }));
    }
} // namespace
