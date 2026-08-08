#include <VSL/dimension.hpp>

#include <gtest/gtest.h>

#include <type_traits>

TEST(VSLDimension, ExposesStaticDimensionValues)
{
    EXPECT_EQ(vsl::D1::dimension, 1);
    EXPECT_EQ(vsl::D2::dimension, 2);
    EXPECT_EQ(vsl::D3::dimension, 3);
}

TEST(VSLDimension, MapsIntegerDimensionToTypes)
{
    static_assert(std::same_as<vsl::dimension<1>::value, vsl::D1>);
    static_assert(std::same_as<vsl::dimension<2>::value, vsl::D2>);
    static_assert(std::same_as<vsl::dimension<3>::value, vsl::D3>);

    SUCCEED();
}

TEST(VSLDimension, RecognizesSupportedDimensionTypes)
{
    static_assert(vsl::is_dimension<vsl::D1>);
    static_assert(vsl::is_dimension<vsl::D2>);
    static_assert(vsl::is_dimension<vsl::D3>);
    static_assert(!vsl::is_dimension<int>);

    SUCCEED();
}
