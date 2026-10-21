#include "support/render_fixture.hpp"
namespace {
    using RenderPassEndVulkanIntegrationTest = vsl_test::RenderVulkanIntegrationTest;
    TEST_F(RenderPassEndVulkanIntegrationTest, EndsPassBeforeTransferReadsItsPixels) {
        ExpectTriangleColor(render([](auto& stream) { stream << vsl::command::Draw{3}; }));
    }
} // namespace
