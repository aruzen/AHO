#include "support/vulkan_fixture.hpp"

namespace {
    using ResourceBindingVulkanIntegrationTest = vsl_test::VulkanIntegrationTest;
    TEST_F(ResourceBindingVulkanIntegrationTest, InjectsLayoutsInDescriptorSetOrder) {
        vsl::graphic_resource::BindingLayout first(
            device, {{0, vsl::graphic_resource::Type::UNIFORM_BUFFER, vsl::ShaderFlag::VERTEX, 1}});
        vsl::graphic_resource::BindingLayout second(
            device, {{2, vsl::graphic_resource::Type::STORAGE_BUFFER, vsl::ShaderFlag::COMPUTE, 1}});
        vsl::pipeline_layout::ResourceBinding binding({first, second});
        ASSERT_EQ(binding.resource_binding_sets.size(), 2u);
        auto layout = make_layout_state();
        binding.injection(layout);
        const auto& info = layout.data_->info->pipeline_layout;
        ASSERT_EQ(info.setLayoutCount, 2u);
        EXPECT_EQ(info.pSetLayouts[0], first.data_->layout);
        EXPECT_EQ(info.pSetLayouts[1], second.data_->layout);
    }
} // namespace
