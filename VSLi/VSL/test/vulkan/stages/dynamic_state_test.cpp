#include "support/vulkan_fixture.hpp"

namespace {
    namespace pl = vsl::pipeline_layout;
    using DynamicStateVulkanIntegrationTest = vsl_test::VulkanIntegrationTest;
    TEST_F(DynamicStateVulkanIntegrationTest, InjectsExpectedPipelineState) {
        auto layout = make_layout_state();

        pl::DynamicState{}.injection(layout);
        const auto& states = layout.data_->info->enabled_dynamic_states;
        EXPECT_EQ(states, (std::vector{vk::DynamicState::eScissor, vk::DynamicState::eViewport}));
        pl::DynamicState{}.injection(layout);
        EXPECT_EQ(layout.data_->info->enabled_dynamic_states.size(), 2u);
    }
} // namespace
