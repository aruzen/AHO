#include "support/compute_fixture.hpp"
namespace {
using PushConstantVulkanIntegrationTest = vsl_test::ComputeVulkanIntegrationTest;
TEST_F(PushConstantVulkanIntegrationTest, TypedAndExplicitSizeOverloadsReachShader) {
    std::uint32_t base = 37;
    vsl::command::PushConstant command(*layout, vsl::ShaderFlag::Compute, base);
    EXPECT_EQ(command.layout._data, layout->_data);
    EXPECT_EQ(command.value, &base);
    EXPECT_EQ(command.size, 4u);
    EXPECT_EQ(command.offset, 0u);
    EXPECT_EQ(command.targetType, vsl::ShaderFlag::Compute);
    expect_sequence(execute(base), base);
    expect_sequence(execute(73, false, true), 73);
}
} // namespace
