#include "support/vulkan_fixture.hpp"

namespace {
    using PipelineLayoutVulkanIntegrationTest = vsl_test::VulkanIntegrationTest;
    TEST_F(PipelineLayoutVulkanIntegrationTest, CreatesIndependentCopyAndFreezesEditableState) {
        vsl::PipelineLayout original(device);
        auto copy = original.copy();
        EXPECT_NE(copy._data->pipelineLayout, original._data->pipelineLayout);
        EXPECT_NE(copy._data->info, original._data->info);
        EXPECT_EQ(copy._data->device, original._data->device);
        vsl::PipelineLayout shared(original._data);
        EXPECT_EQ(shared._data, original._data);
        auto frozen = copy.freeze();
        EXPECT_EQ(frozen._data, copy._data);
        EXPECT_EQ(frozen._data->info, nullptr);
        EXPECT_TRUE(frozen._data->pipelineLayout);
        EXPECT_NE(original._data->info, nullptr);
    }
    TEST_F(PipelineLayoutVulkanIntegrationTest, AddsStateAndCreatesUsableLayoutHandle) {
        vsl::PipelineLayout layout(device);
        const auto replaced_handle = layout._data->pipelineLayout;
        auto added = layout.add(vsl::pipeline_layout::Rasterization(vsl::pipeline_layout::CullingMode::Front));
        device._data->device.destroyPipelineLayout(replaced_handle); // I19: add replaces without destroying.
        EXPECT_EQ(added._data, layout._data);
        EXPECT_TRUE(layout._data->pipelineLayout);
        EXPECT_EQ(layout._data->info->rasterization.cullMode, vk::CullModeFlagBits::eFront);
    }
} // namespace
