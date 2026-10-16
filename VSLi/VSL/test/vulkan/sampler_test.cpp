#include "support/vulkan_fixture.hpp"

namespace {
using SamplerVulkanIntegrationTest = vsl_test::VulkanIntegrationTest;
TEST_F(SamplerVulkanIntegrationTest, CreatesNearestSamplerWithoutAnisotropyRequirement) {
    vsl::Sampler sampler(device, vsl::SamplingMode::Nearest, vsl::EdgeMode::ClampToEdge);
    EXPECT_TRUE(sampler._data->sampler);
    EXPECT_EQ(sampler._data->device, device._data);
}
TEST_F(SamplerVulkanIntegrationTest, CreatesDefaultLinearSamplerWhenFeatureIsAvailable) {
    if (!physical._data->features->samplerAnisotropy)
        GTEST_SKIP() << "samplerAnisotropy is not supported";
    vsl::Sampler sampler(device);
    EXPECT_TRUE(sampler._data->sampler);
}
} // namespace
