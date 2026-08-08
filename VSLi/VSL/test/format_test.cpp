#include <VSL/format.hpp>

#include <gtest/gtest.h>

#include <cstdint>

TEST(VSLFormat, ConvertsPrimitiveTypes)
{
    using namespace vsl::data_format;

    static_assert(convert_graphic_type<int>() == Int);
    static_assert(convert_graphic_type<unsigned int>() == UnsignedInt);
    static_assert(convert_graphic_type<float>() == Float);
    static_assert(convert_graphic_type<double>() == Float64);

    EXPECT_EQ(convert_graphic_type<int>().size(), 4u);
    EXPECT_EQ(convert_graphic_type<double>().size(), 8u);
}

TEST(VSLFormat, ConvertsScalarFormatToVectorFormat)
{
    using namespace vsl::data_format;

    constexpr auto float3 = Float.toVec<3>();
    const auto int4 = Int.toVec(4);

    EXPECT_EQ(float3.type(), ___FormatTypes::SignedFloat32Vec3);
    EXPECT_EQ(float3.size(), 12u);
    EXPECT_EQ(int4.type(), ___FormatTypes::SignedInt32Vec4);
    EXPECT_EQ(int4.size(), 16u);
}

TEST(VSLFormat, ReturnsUndefinedForUnsupportedVectorLength)
{
    using namespace vsl::data_format;

    const auto result = Float.toVec(5);

    EXPECT_EQ(result, Undefined);
}

TEST(VSLFormat, DISABLED_SignedNormalized8RGBShouldHaveThreeByteSize)
{
    using namespace vsl::data_format;

    // Currently documents a suspicious format table entry:
    // SignedNormalized8RGB is defined with size 3 * 3 * 1.
    EXPECT_EQ(SignedNormalized8RGB.size(), 3u);
}
