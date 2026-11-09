#include <VSL/lifetime/descriptor_access.hpp>
#include "support/vulkan_fixture.hpp"

namespace {
    using VSL_NAMESPACE::lifetime::detail::DescriptorAccess;
}

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
        const auto& info = vsl_test::inspect_layout(layout).pipeline_layout;
        ASSERT_EQ(info.setLayoutCount, 2u);
        EXPECT_EQ(info.pSetLayouts[0], DescriptorAccess::Owned(first)->layout);
        EXPECT_EQ(info.pSetLayouts[1], DescriptorAccess::Owned(second)->layout);
    }
} // namespace
