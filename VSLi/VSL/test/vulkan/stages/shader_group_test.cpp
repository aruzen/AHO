#include "support/vulkan_fixture.hpp"

namespace {
using ShaderGroupVulkanIntegrationTest = vsl_test::VulkanIntegrationTest;
TEST_F(ShaderGroupVulkanIntegrationTest, InjectsInitialAndAppendedShadersInOrder) {
    vsl::Shader<vsl::ShaderType::Vertex> vertex(device, vsl_test::shader_path("triangle.vert.spv"));
    vsl::Shader<vsl::ShaderType::Fragment> fragment(device, vsl_test::shader_path("triangle.frag.spv"));
    vsl::pipeline_layout::ShaderGroup group("triangle", {vertex});
    group.addShader(fragment);
    ASSERT_EQ(group._data->shaders.size(), 2u);
    EXPECT_EQ(group._data->name, "triangle");
    auto layout = make_layout_state();
    group.injection(layout);
    ASSERT_EQ(layout._data->info->shaderStages.size(), 2u);
    EXPECT_EQ(layout._data->info->shaderStages[0].module, vertex._data->shaderModule);
    EXPECT_EQ(layout._data->info->shaderStages[1].module, fragment._data->shaderModule);
}
TEST_F(ShaderGroupVulkanIntegrationTest, DISABLED_RemovesMatchingShaderByNameAndHandle) {
    vsl::Shader<vsl::ShaderType::Vertex> vertex(device, vsl_test::shader_path("triangle.vert.spv"),
                                                "vertex_entry");
    vsl::Shader<vsl::ShaderType::Fragment> fragment(device, vsl_test::shader_path("triangle.frag.spv"),
                                                    "fragment_entry");
    vsl::pipeline_layout::ShaderGroup group("triangle", {vertex, fragment});
    group.removeShader("vertex_entry");
    EXPECT_EQ(group._data->shaders.size(), 1u);
    group.removeShader(fragment);
    EXPECT_TRUE(group._data->shaders.empty());
}
} // namespace
