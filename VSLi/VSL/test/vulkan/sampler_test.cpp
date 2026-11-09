#include <VSL/lifetime/sampler_access.hpp>
#include "support/vulkan_fixture.hpp"

namespace {
    using VSL_NAMESPACE::lifetime::detail::SamplerAccess;
}

namespace {
    using SamplerVulkanIntegrationTest = vsl_test::VulkanIntegrationTest;
    TEST_F(SamplerVulkanIntegrationTest, CreatesNearestSamplerWithoutAnisotropyRequirement) {
        vsl::Sampler sampler(device, vsl::SamplingMode::NEAREST, vsl::EdgeMode::CLAMP_TO_EDGE);
        EXPECT_TRUE(SamplerAccess::Owned(sampler)->sampler);
        EXPECT_EQ(SamplerAccess::Owned(sampler)->device, device.data_.get());
    }
    TEST_F(SamplerVulkanIntegrationTest, CreatesDefaultLinearSamplerWhenFeatureIsAvailable) {
        if (!physical.data_->features->samplerAnisotropy)
            GTEST_SKIP() << "samplerAnisotropy is not supported";
        vsl::Sampler sampler(device);
        EXPECT_TRUE(SamplerAccess::Owned(sampler)->sampler);
    }
} // namespace
