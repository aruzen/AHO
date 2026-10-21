#include "support/vulkan_fixture.hpp"

namespace {
    using SamplerVulkanIntegrationTest = vsl_test::VulkanIntegrationTest;
    TEST_F(SamplerVulkanIntegrationTest, CreatesNearestSamplerWithoutAnisotropyRequirement) {
        vsl::Sampler sampler(device, vsl::SamplingMode::NEAREST, vsl::EdgeMode::CLAMP_TO_EDGE);
        EXPECT_TRUE(sampler.data_->sampler);
        EXPECT_EQ(sampler.data_->device, device.data_);
    }
    TEST_F(SamplerVulkanIntegrationTest, CreatesDefaultLinearSamplerWhenFeatureIsAvailable) {
        if (!physical.data_->features->samplerAnisotropy)
            GTEST_SKIP() << "samplerAnisotropy is not supported";
        vsl::Sampler sampler(device);
        EXPECT_TRUE(sampler.data_->sampler);
    }
} // namespace
