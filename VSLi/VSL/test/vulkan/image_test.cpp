#include <VSL/lifetime/image_access.hpp>
#include "support/vulkan_fixture.hpp"

namespace {
    using VSL_NAMESPACE::lifetime::detail::ImageAccess;
}

namespace {
    using ImageVulkanIntegrationTest = vsl_test::VulkanIntegrationTest;
    TEST_F(ImageVulkanIntegrationTest, CreatesTwoDimensionalImageAndViewWithExpectedSize) {
        vsl::Image<vsl::ImageType::TRANSFER_DST | vsl::ImageType::SAMPLED> image(
            device, 7, 5, vsl::data_format::UNSIGNED_INT8_RGBA);
        EXPECT_TRUE(ImageAccess::Owned(image)->image);
        EXPECT_TRUE(ImageAccess::Owned(image)->view);
        EXPECT_TRUE(ImageAccess::Owned(image)->memory);
        EXPECT_EQ(ImageAccess::Owned(image)->width, 7u);
        EXPECT_EQ(ImageAccess::Owned(image)->height, 5u);
        EXPECT_EQ(ImageAccess::Owned(image)->count, 1u);
        EXPECT_EQ(image.size(), 7u * 5u * 4u);
    }
    TEST_F(ImageVulkanIntegrationTest, CreatesArrayImageWithRequestedLayerCount) {
        vsl::Image<vsl::ImageType::TRANSFER_DST | vsl::ImageType::SAMPLED> image(
            device, 4, 4, vsl::data_format::UNSIGNED_INT8_RGBA, 3);
        EXPECT_EQ(ImageAccess::Owned(image)->count, 3u);
        EXPECT_TRUE(ImageAccess::Owned(image)->view);
    }
    TEST_F(ImageVulkanIntegrationTest, ArraySizeIncludesAllLayers) {
        vsl::Image<vsl::ImageType::TRANSFER_DST | vsl::ImageType::SAMPLED> image(
            device, 4, 4, vsl::data_format::UNSIGNED_INT8_RGBA, 3);
        EXPECT_EQ(image.size(), 4u * 4u * 4u * 3u);
    }
} // namespace

namespace {
    TEST_F(ImageVulkanIntegrationTest, UnavailableImageMemoryPropertiesReleasePartiallyCreatedImages) {
        using Unsupported = vsl::Image<vsl::ImageType::TRANSFER_DST | vsl::ImageType::SAMPLED,
                                       vsl::MemoryProperty::HOST_VISIBLE | vsl::MemoryProperty::LAZILY_ALLOCATED>;
        for (int attempt = 0; attempt < 3; ++attempt)
            EXPECT_THROW((Unsupported(device, 2, 2, vsl::data_format::UNSIGNED_NORMALIZED8_RGBA)),
                         vsl::exceptions::RuntimeException);
        vsl::Image<vsl::ImageType::TRANSFER_DST | vsl::ImageType::SAMPLED> valid(
            device, 2, 2, vsl::data_format::UNSIGNED_NORMALIZED8_RGBA);
        EXPECT_TRUE(ImageAccess::Owned(valid)->image);
        EXPECT_TRUE(ImageAccess::Owned(valid)->view);
        EXPECT_TRUE(ImageAccess::Owned(valid)->memory);
    }
} // namespace
