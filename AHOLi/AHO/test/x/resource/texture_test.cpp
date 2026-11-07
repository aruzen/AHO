#ifdef AHO_GPU_TESTS
#include <AHO/x/resource/texture.hpp>
#include <AHO/x/command/texture_update.hpp>
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
        EXPECT_EQ(texture.format(), vsl::data_format::UNSIGNED_NORMALIZED8_RGBA);
        EXPECT_THROW((resource::Texture(device, 0, 1)), std::invalid_argument);
        resource::Texture color(device, 1, 1, vsl::data_format::SRGB8_RGBA);
        EXPECT_EQ(color.format(), vsl::data_format::SRGB8_RGBA);
        EXPECT_THROW((resource::Texture(device, 1, 1, vsl::data_format::FLOAT_RGBA)), std::invalid_argument);
    }
    TEST_F(ExperimentalTextureVulkanIntegrationTest, NativeImageUseBlocksAHOUpdatesAndBothEntrypointsSharePhysicalState) {
        aho::x::resource::Texture texture(device, 1, 1);
        aho::resource::Buffer<aho::resource::BufferType::GENERAL> pixels(4);
        auto image = texture.image();
        const auto use = texture.resource_use();
        EXPECT_EQ(use.state.get(), &image.use_state());
        ASSERT_TRUE(use.constraint_state);
        EXPECT_NE(use.constraint_state, use.state);
        auto native_manager = make_manager(), aho_manager = make_manager();
        auto native = native_manager.start_phase<vsl::ComputePhase>();
        image.resource_use().record(native_manager.get_buffer());
        auto update = aho_manager.start_phase<vsl::ComputePhase>();
        EXPECT_THROW(update << texture.update(pixels), std::logic_error);
        EXPECT_FALSE(use.constraint_state->busy());
        native.cancel();
        EXPECT_NO_THROW(update << texture.update(pixels));
        EXPECT_TRUE(image.use_state().busy());
        EXPECT_TRUE(use.constraint_state->busy());
        update.cancel();
        EXPECT_FALSE(image.use_state().busy());
        EXPECT_FALSE(use.constraint_state->busy());
    }
} // namespace
#endif
