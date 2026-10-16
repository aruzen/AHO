#include "support/vulkan_fixture.hpp"

namespace {
using ResourceBindingVulkanIntegrationTest = vsl_test::VulkanIntegrationTest;
TEST_F(ResourceBindingVulkanIntegrationTest, InjectsLayoutsInDescriptorSetOrder) {
    vsl::graphic_resource::BindingLayout first(
        device, {{0, vsl::graphic_resource::Type::UniformBuffer, vsl::ShaderFlag::Vertex, 1}});
    vsl::graphic_resource::BindingLayout second(
        device, {{2, vsl::graphic_resource::Type::StorageBuffer, vsl::ShaderFlag::Compute, 1}});
    vsl::pipeline_layout::ResourceBinding binding({first, second});
    ASSERT_EQ(binding.resourceBindingSets.size(), 2u);
    auto layout = make_layout_state();
    binding.injection(layout);
    const auto &info = layout._data->info->pipelineLayout;
    ASSERT_EQ(info.setLayoutCount, 2u);
    EXPECT_EQ(info.pSetLayouts[0], first._data->layout);
    EXPECT_EQ(info.pSetLayouts[1], second._data->layout);
}
} // namespace
