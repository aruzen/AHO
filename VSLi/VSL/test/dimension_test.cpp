#include <gtest/gtest.h>
#include <VSL/dimension.hpp>
#include <type_traits>

namespace {
    struct PretendDimension {
        static constexpr char dimension = 2;
    };
    static_assert(vsl::is_dimension<vsl::D1> && vsl::is_dimension<vsl::D2> && vsl::is_dimension<vsl::D3>);
    static_assert(!vsl::is_dimension<int> && !vsl::is_dimension<PretendDimension>);
    static_assert(std::is_same_v<vsl::dimension<1>::value, vsl::D1>);
    static_assert(std::is_same_v<vsl::dimension<2>::value, vsl::D2>);
    static_assert(std::is_same_v<vsl::dimension<3>::value, vsl::D3>);

    TEST(DimensionTest, SupportedDimensionsHaveExpectedValues) {
        EXPECT_EQ(vsl::D1::dimension, 1);
        EXPECT_EQ(vsl::D2::dimension, 2);
        EXPECT_EQ(vsl::D3::dimension, 3);
    }
} // namespace
