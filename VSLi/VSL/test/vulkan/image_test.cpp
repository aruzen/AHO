#include "support/vulkan_fixture.hpp"

namespace {
using ImageVulkanIntegrationTest = vsl_test::VulkanIntegrationTest;
TEST_F(ImageVulkanIntegrationTest, CreatesTwoDimensionalImageAndViewWithExpectedSize) {
    vsl::Image<vsl::ImageType::TransferDst | vsl::ImageType::Sampled> image(
        device, 7, 5, vsl::data_format::UnsignedInt8RGBA);
    EXPECT_TRUE(image._data->image);
    EXPECT_TRUE(image._data->view);
    EXPECT_TRUE(image._data->memory);
    EXPECT_EQ(image._data->width, 7u);
    EXPECT_EQ(image._data->height, 5u);
    EXPECT_EQ(image._data->count, 1u);
    EXPECT_EQ(image.size(), 7u * 5u * 4u);
}
TEST_F(ImageVulkanIntegrationTest, CreatesArrayImageWithRequestedLayerCount) {
    vsl::Image<vsl::ImageType::TransferDst | vsl::ImageType::Sampled> image(
        device, 4, 4, vsl::data_format::UnsignedInt8RGBA, 3);
    EXPECT_EQ(image._data->count, 3u);
    EXPECT_TRUE(image._data->view);
}
TEST_F(ImageVulkanIntegrationTest, ArraySizeIncludesAllLayers) {
    vsl::Image<vsl::ImageType::TransferDst | vsl::ImageType::Sampled> image(
        device, 4, 4, vsl::data_format::UnsignedInt8RGBA, 3);
    EXPECT_EQ(image.size(), 4u * 4u * 4u * 3u);
}
}
