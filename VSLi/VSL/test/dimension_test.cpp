#include <gtest/gtest.h>
#include <VSL/dimension.hpp>
#include <type_traits>

namespace {
    struct PretendDimension {
        static constexpr char DIMENSION = 2;
    };
    static_assert(vsl::is_dimension<vsl::D1> && vsl::is_dimension<vsl::D2> && vsl::is_dimension<vsl::D3>);
    static_assert(!vsl::is_dimension<int> && !vsl::is_dimension<PretendDimension>);
    static_assert(std::is_same_v<vsl::Dimension<1>::Value, vsl::D1>);
    static_assert(std::is_same_v<vsl::Dimension<2>::Value, vsl::D2>);
    static_assert(std::is_same_v<vsl::Dimension<3>::Value, vsl::D3>);
    static_assert(vsl::is_dimension<vsl::D4>);
    static_assert(vsl::is_image_dimension<vsl::D1> && vsl::is_image_dimension<vsl::D2> && vsl::is_image_dimension<vsl::D3>);
    static_assert(!vsl::is_image_dimension<vsl::D4> && !vsl::is_image_dimension<int>);
    static_assert(std::is_same_v<vsl::Dimension<4>::Value, vsl::D4>);

    TEST(DimensionTest, SupportedDimensionsHaveExpectedValues) {
        EXPECT_EQ(vsl::D1::DIMENSION, 1);
        EXPECT_EQ(vsl::D2::DIMENSION, 2);
        EXPECT_EQ(vsl::D3::DIMENSION, 3);
        EXPECT_EQ(vsl::D4::DIMENSION, 4);
    }
} // namespace
