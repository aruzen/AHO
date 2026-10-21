#include "support/vulkan_fixture.hpp"

namespace {
    using ShaderGroupVulkanIntegrationTest = vsl_test::VulkanIntegrationTest;
    TEST_F(ShaderGroupVulkanIntegrationTest, InjectsInitialAndAppendedShadersInOrder) {
        vsl::Shader<vsl::ShaderType::VERTEX> vertex(device, vsl_test::shader_path("triangle.vert.spv"));
        vsl::Shader<vsl::ShaderType::FRAGMENT> fragment(device, vsl_test::shader_path("triangle.frag.spv"));
        vsl::pipeline_layout::ShaderGroup group("triangle", {vertex});
        group.add_shader(fragment);
        ASSERT_EQ(group.data_->shaders.size(), 2u);
        EXPECT_EQ(group.data_->name, "triangle");
        auto layout = make_layout_state();
        group.injection(layout);
        ASSERT_EQ(layout.data_->info->shader_stages.size(), 2u);
        EXPECT_EQ(layout.data_->info->shader_stages[0].module, vertex.data_->shader_module);
        EXPECT_EQ(layout.data_->info->shader_stages[1].module, fragment.data_->shader_module);
    }
    TEST_F(ShaderGroupVulkanIntegrationTest, RemovesMatchingShaderByNameAndHandle) {
        vsl::Shader<vsl::ShaderType::VERTEX> vertex(device, vsl_test::shader_path("triangle.vert.spv"),
                                                    "vertex_entry");
        vsl::Shader<vsl::ShaderType::FRAGMENT> fragment(device, vsl_test::shader_path("triangle.frag.spv"),
                                                        "fragment_entry");
        vsl::pipeline_layout::ShaderGroup group("triangle", {vertex, fragment});
        group.remove_shader("vertex_entry");
        EXPECT_EQ(group.data_->shaders.size(), 1u);
        group.remove_shader(fragment);
        EXPECT_TRUE(group.data_->shaders.empty());
    }
    TEST_F(ShaderGroupVulkanIntegrationTest, EmptyGroupsAllowBothRemovalOverloads) {
        vsl::Shader<vsl::ShaderType::VERTEX> vertex(device, vsl_test::shader_path("triangle.vert.spv"));
        vsl::pipeline_layout::ShaderGroup group("empty", {});
        group.remove_shader("missing");
        group.remove_shader(vertex);
        EXPECT_TRUE(group.data_->shaders.empty());
    }
    TEST_F(ShaderGroupVulkanIntegrationTest, NameRemovalErasesEveryMatchAndKeepsOtherEntriesInOrder) {
        vsl::Shader<vsl::ShaderType::VERTEX> vertex(device, vsl_test::shader_path("triangle.vert.spv"), "shared");
        vsl::Shader<vsl::ShaderType::FRAGMENT> fragment(device, vsl_test::shader_path("triangle.frag.spv"), "shared");
        vsl::Shader<vsl::ShaderType::VERTEX> other(device, vsl_test::shader_path("triangle.vert.spv"), "other");
        vsl::pipeline_layout::ShaderGroup group("duplicates", {vertex, fragment, other, vertex, other});
        group.remove_shader("shared");
        ASSERT_EQ(group.data_->shaders.size(), 2u);
        EXPECT_EQ(group.data_->shaders[0], other.data_);
        EXPECT_EQ(group.data_->shaders[1], other.data_);
        group.remove_shader("other");
        EXPECT_TRUE(group.data_->shaders.empty());
        group.remove_shader("other");
    }
    TEST_F(ShaderGroupVulkanIntegrationTest, HandleRemovalKeepsDifferentModulesWithTheSameName) {
        vsl::Shader<vsl::ShaderType::VERTEX> first(device, vsl_test::shader_path("triangle.vert.spv"), "same");
        vsl::Shader<vsl::ShaderType::VERTEX> second(device, vsl_test::shader_path("triangle.vert.spv"), "same");
        vsl::pipeline_layout::ShaderGroup group("duplicates", {first, second, first, second});
        group.remove_shader(first);
        ASSERT_EQ(group.data_->shaders.size(), 2u);
        EXPECT_EQ(group.data_->shaders[0], second.data_);
        EXPECT_EQ(group.data_->shaders[1], second.data_);
        group.remove_shader(second);
        EXPECT_TRUE(group.data_->shaders.empty());
        group.remove_shader(second);
    }
    TEST_F(ShaderGroupVulkanIntegrationTest, MissingMatchesPreserveOrderAndInjectedStages) {
        vsl::Shader<vsl::ShaderType::VERTEX> vertex(device, vsl_test::shader_path("triangle.vert.spv"));
        vsl::Shader<vsl::ShaderType::FRAGMENT> fragment(device, vsl_test::shader_path("triangle.frag.spv"));
        vsl::Shader<vsl::ShaderType::VERTEX> missing(device, vsl_test::shader_path("triangle.vert.spv"));
        vsl::pipeline_layout::ShaderGroup group("unchanged", {vertex, fragment});
        group.remove_shader("missing");
        group.remove_shader(missing);
        ASSERT_EQ(group.data_->shaders.size(), 2u);
        EXPECT_EQ(group.data_->shaders[0], vertex.data_);
        EXPECT_EQ(group.data_->shaders[1], fragment.data_);
        auto layout = make_layout_state();
        group.injection(layout);
        ASSERT_EQ(layout.data_->info->shader_stages.size(), 2u);
        EXPECT_EQ(layout.data_->info->shader_stages[0].module, vertex.data_->shader_module);
        EXPECT_EQ(layout.data_->info->shader_stages[1].module, fragment.data_->shader_module);
    }
} // namespace
