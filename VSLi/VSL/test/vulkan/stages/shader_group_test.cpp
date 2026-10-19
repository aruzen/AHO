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
    TEST_F(ShaderGroupVulkanIntegrationTest, RemovesMatchingShaderByNameAndHandle) {
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
    TEST_F(ShaderGroupVulkanIntegrationTest, EmptyGroupsAllowBothRemovalOverloads) {
        vsl::Shader<vsl::ShaderType::Vertex> vertex(device, vsl_test::shader_path("triangle.vert.spv"));
        vsl::pipeline_layout::ShaderGroup group("empty", {});
        group.removeShader("missing");
        group.removeShader(vertex);
        EXPECT_TRUE(group._data->shaders.empty());
    }
    TEST_F(ShaderGroupVulkanIntegrationTest, NameRemovalErasesEveryMatchAndKeepsOtherEntriesInOrder) {
        vsl::Shader<vsl::ShaderType::Vertex> vertex(device, vsl_test::shader_path("triangle.vert.spv"), "shared");
        vsl::Shader<vsl::ShaderType::Fragment> fragment(device, vsl_test::shader_path("triangle.frag.spv"), "shared");
        vsl::Shader<vsl::ShaderType::Vertex> other(device, vsl_test::shader_path("triangle.vert.spv"), "other");
        vsl::pipeline_layout::ShaderGroup group("duplicates", {vertex, fragment, other, vertex, other});
        group.removeShader("shared");
        ASSERT_EQ(group._data->shaders.size(), 2u);
        EXPECT_EQ(group._data->shaders[0], other._data);
        EXPECT_EQ(group._data->shaders[1], other._data);
        group.removeShader("other");
        EXPECT_TRUE(group._data->shaders.empty());
        group.removeShader("other");
    }
    TEST_F(ShaderGroupVulkanIntegrationTest, HandleRemovalKeepsDifferentModulesWithTheSameName) {
        vsl::Shader<vsl::ShaderType::Vertex> first(device, vsl_test::shader_path("triangle.vert.spv"), "same");
        vsl::Shader<vsl::ShaderType::Vertex> second(device, vsl_test::shader_path("triangle.vert.spv"), "same");
        vsl::pipeline_layout::ShaderGroup group("duplicates", {first, second, first, second});
        group.removeShader(first);
        ASSERT_EQ(group._data->shaders.size(), 2u);
        EXPECT_EQ(group._data->shaders[0], second._data);
        EXPECT_EQ(group._data->shaders[1], second._data);
        group.removeShader(second);
        EXPECT_TRUE(group._data->shaders.empty());
        group.removeShader(second);
    }
    TEST_F(ShaderGroupVulkanIntegrationTest, MissingMatchesPreserveOrderAndInjectedStages) {
        vsl::Shader<vsl::ShaderType::Vertex> vertex(device, vsl_test::shader_path("triangle.vert.spv"));
        vsl::Shader<vsl::ShaderType::Fragment> fragment(device, vsl_test::shader_path("triangle.frag.spv"));
        vsl::Shader<vsl::ShaderType::Vertex> missing(device, vsl_test::shader_path("triangle.vert.spv"));
        vsl::pipeline_layout::ShaderGroup group("unchanged", {vertex, fragment});
        group.removeShader("missing");
        group.removeShader(missing);
        ASSERT_EQ(group._data->shaders.size(), 2u);
        EXPECT_EQ(group._data->shaders[0], vertex._data);
        EXPECT_EQ(group._data->shaders[1], fragment._data);
        auto layout = make_layout_state();
        group.injection(layout);
        ASSERT_EQ(layout._data->info->shaderStages.size(), 2u);
        EXPECT_EQ(layout._data->info->shaderStages[0].module, vertex._data->shaderModule);
        EXPECT_EQ(layout._data->info->shaderStages[1].module, fragment._data->shaderModule);
    }
} // namespace
