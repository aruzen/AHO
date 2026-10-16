#include "support/vulkan_fixture.hpp"

namespace {
using DepthStencilVulkanIntegrationTest = vsl_test::VulkanIntegrationTest;
// U01: 実装が TODO のため通常実行から除外。状態を設定する契約だけを検証する。
TEST_F(DepthStencilVulkanIntegrationTest, DISABLED_ProvidesDepthStencilStateWhenInjected) {
    auto layout = make_layout_state();
    vsl::pipeline_layout::DepthStencil{}.injection(layout);
    EXPECT_TRUE(layout._data->info->depthStencil.has_value());
}
} // namespace
