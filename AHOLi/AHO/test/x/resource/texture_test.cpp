#ifdef AHO_GPU_TESTS
#include <AHO/x/resource/texture.hpp>
#include "support/vulkan_fixture.hpp"

namespace {
    class ExperimentalTextureVulkanIntegrationTest : public vsl_test::VulkanIntegrationTest {
        bool validate_vulkan() const override;
    };

    bool ExperimentalTextureVulkanIntegrationTest::validate_vulkan() const {
        return true;
    }

    TEST_F(ExperimentalTextureVulkanIntegrationTest, ConstructorReportsMetadataAndRejectsUnsupportedExtentsAndFormats) {
        namespace resource = aho::x::resource;
        resource::Texture texture(device, 4, 3);
        EXPECT_EQ(texture.width(), 4u);
        EXPECT_EQ(texture.height(), 3u);
        EXPECT_EQ(texture.format(), vsl::data_format::UnsignedNormalized8RGBA);
        EXPECT_THROW((resource::Texture(device, 0, 1)), std::invalid_argument);
        EXPECT_THROW((resource::Texture(device, 1, 1, vsl::data_format::Srgb8RGBA)), std::invalid_argument);
    }
} // namespace
#endif
