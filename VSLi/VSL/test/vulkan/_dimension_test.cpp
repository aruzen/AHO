#include "support/vulkan_fixture.hpp"
#include <VSL/vulkan/_dimension.hpp>

namespace {
    static_assert(vsl::helper::dimension::to_image_view_type<vsl::D1>() == vk::ImageViewType::e1D);
    static_assert(vsl::helper::dimension::to_image_view_type<vsl::D2>() == vk::ImageViewType::e2D);
    static_assert(vsl::helper::dimension::to_image_view_type<vsl::D3>() == vk::ImageViewType::e3D);
    TEST(ImageViewDimensionTest, UnsupportedDimensionUsesConfiguredDefault) {
        EXPECT_EQ(vsl::helper::dimension::to_image_view_type<void>(),
                  vsl::helper::dimension::to_image_view_type<vsl::VSL_DEFAULT_dimension_STRUCT>());
    }
} // namespace
