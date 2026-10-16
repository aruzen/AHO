#include "support/compute_fixture.hpp"
namespace {
using BindGraphicResourceVulkanIntegrationTest = vsl_test::ComputeVulkanIntegrationTest;
TEST_F(BindGraphicResourceVulkanIntegrationTest, SingleAndVectorResourcesBindToComputeShader) {
    vsl::command::BindGraphicResource command(resource, vsl::graphic_resource::BindingDestination::Compute);
    ASSERT_EQ(command.resources.size(), 1u);
    EXPECT_EQ(command.resources.front()._data, resource._data);
    EXPECT_EQ(command.destination, vsl::graphic_resource::BindingDestination::Compute);
    EXPECT_EQ(command.first_binding, 0u);
    EXPECT_FALSE(command.pipeline.has_value());
    command.setPipeline(*pipeline);
    ASSERT_TRUE(command.pipeline.has_value());
    EXPECT_EQ(command.pipeline->_data, pipeline->_data);
    expect_sequence(execute(13), 13);
    expect_sequence(execute(29, true), 29);
}
} // namespace
