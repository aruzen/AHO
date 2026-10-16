#include "support/vulkan_fixture.hpp"
#include <VSL/vulkan/_dimension.hpp>

namespace {
static_assert(vsl::helper::dimension::toImageViewType<vsl::D1>() == vk::ImageViewType::e1D);
static_assert(vsl::helper::dimension::toImageViewType<vsl::D2>() == vk::ImageViewType::e2D);
static_assert(vsl::helper::dimension::toImageViewType<vsl::D3>() == vk::ImageViewType::e3D);
TEST(ImageViewDimensionTest, UnsupportedDimensionUsesConfiguredDefault) {
    EXPECT_EQ(vsl::helper::dimension::toImageViewType<void>(),
              vsl::helper::dimension::toImageViewType<vsl::VSL_DEFAULT_dimension_STRUCT>());
}
} // namespace
