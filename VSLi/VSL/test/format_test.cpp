#include <gtest/gtest.h>
#include <VSL/format.hpp>
#include <array>

namespace {
    namespace format = vsl::data_format;
    struct Family {
        std::array<format::___Format, 4> vectors;
        std::uint32_t component_bytes;
        const char *name;
    };
#define FAMILY(name, bytes)                                                                      \
    Family {                                                                                     \
        {format::name##R, format::name##RG, format::name##RGB, format::name##RGBA}, bytes, #name \
    }
    const std::array families{FAMILY(UnsignedNormalized8, 1),
                              FAMILY(SignedNormalized8, 1),
                              FAMILY(UnsignedScaled8, 1),
                              FAMILY(SignedScaled8, 1),
                              FAMILY(UnsignedInt8, 1),
                              FAMILY(SignedInt8, 1),
                              FAMILY(Srgb8, 1),
                              FAMILY(UnsignedNormalized16, 2),
                              FAMILY(SignedNormalized16, 2),
                              FAMILY(UnsignedScaled16, 2),
                              FAMILY(SignedScaled16, 2),
                              FAMILY(UnsignedInt16, 2),
                              FAMILY(SignedInt16, 2),
                              FAMILY(SignedFloat16, 2),
                              FAMILY(UnsignedInt32, 4),
                              FAMILY(SignedInt32, 4),
                              FAMILY(SignedFloat32, 4),
                              FAMILY(UnsignedInt64, 8),
                              FAMILY(SignedInt64, 8),
                              FAMILY(SignedFloat64, 8)};
#undef FAMILY
    class FormatFamilyTest : public testing::TestWithParam<Family> {};
    TEST_P(FormatFamilyTest, ConvertsBetweenScalarAndVectorTypes) {
        const auto &family = GetParam();
        for (std::size_t source = 0; source < 4; ++source) {
            const auto value = family.vectors[source];
            SCOPED_TRACE(source);
            EXPECT_EQ(value.getVecLength(), source + 1);
            EXPECT_EQ(format::convert(value.type()), value);
            EXPECT_EQ(value.toVec<1>().type(), family.vectors[0].type());
            EXPECT_EQ(value.toVec<2>().type(), family.vectors[1].type());
            EXPECT_EQ(value.toVec<3>().type(), family.vectors[2].type());
            EXPECT_EQ(value.toVec<4>().type(), family.vectors[3].type());
            for (std::size_t target = 1; target <= 4; ++target)
                EXPECT_EQ(value.toVec(target).type(), family.vectors[target - 1].type());
        }
    }
    TEST_P(FormatFamilyTest, ByteSizeMatchesComponentCountAndWidth) {
        const auto &family = GetParam();
        for (std::size_t i = 0; i < 4; ++i) {
            EXPECT_EQ(family.vectors[i].size(), family.component_bytes * (i + 1));
        }
    }
    INSTANTIATE_TEST_SUITE_P(StandardFormats, FormatFamilyTest, testing::ValuesIn(families),
                             [](const testing::TestParamInfo<Family> &info) { return info.param.name; });
    TEST(FormatTest, UndefinedAndUnsupportedLengthsHaveNoStorage) {
        EXPECT_EQ(format::Undefined.size(), 0u);
        EXPECT_EQ(format::Undefined.getVecLength(), 0u);
        EXPECT_EQ(format::Undefined.toVec<4>(), format::Undefined);
        EXPECT_EQ(format::Float.toVec(0), format::Undefined);
        EXPECT_EQ(format::Float.toVec(5), format::Undefined);
        EXPECT_EQ(format::Float.toVec<0>(), format::Undefined);
        EXPECT_EQ(format::Float.toVec<5>(), format::Undefined);
        EXPECT_EQ(format::convert(static_cast<format::___FormatTypes>(0xffffffffu)), format::Undefined);
    }
    TEST(FormatTest, ConstructionComparisonAndCommonAliasesPreserveMeaning) {
        const format::___Format a(format::___FormatTypes::SignedFloat32RG, 8);
        const format::___Format b(format::___FormatTypes::SignedFloat32RG, 8);
        EXPECT_EQ(a, b);
        EXPECT_EQ(a.type(), format::___FormatTypes::SignedFloat32RG);
        EXPECT_EQ(a.size(), 8u);
        EXPECT_NE(a, format::FloatRGB);
        EXPECT_TRUE((a <=> b) == 0);
        EXPECT_EQ(format::Float, format::SignedFloat32R);
        EXPECT_EQ(format::IntVec4, format::SignedInt32RGBA);
        EXPECT_EQ(format::UnsignedInt, format::UnsignedInt32R);
    }
    struct StaticFormat {
        static constexpr auto graphic_type = format::FloatRG;
    };
    struct DynamicFormat {
        consteval auto get_graphic_type() const {
            return format::UnsignedInt;
        }
    };
    static_assert(vsl::concepts::is_static_convertible_graphic_type<StaticFormat>);
    static_assert(!vsl::concepts::is_static_convertible_graphic_type<DynamicFormat>);
    static_assert(vsl::concepts::is_convertible_graphic_type<DynamicFormat>);
    static_assert(!vsl::concepts::is_convertible_graphic_type<void *>);
    static_assert(format::convert_graphic_type<StaticFormat>() == format::FloatRG);
    static_assert(format::convert_graphic_type(DynamicFormat{}) == format::UnsignedInt);
    TEST(FormatTest, BuiltinTypesMapToStorageOfTheirWidth) {
        EXPECT_EQ(format::convert_graphic_type<char>(), format::Int8);
        EXPECT_EQ(format::convert_graphic_type<unsigned char>(), format::UnsignedInt8);
        EXPECT_EQ(format::convert_graphic_type<short>(), format::Int16);
        EXPECT_EQ(format::convert_graphic_type<unsigned short>(), format::UnsignedInt16);
        EXPECT_EQ(format::convert_graphic_type<int>(), format::Int);
        EXPECT_EQ(format::convert_graphic_type<unsigned int>(), format::UnsignedInt);
        EXPECT_EQ(format::convert_graphic_type<long>().size(), sizeof(long));
        EXPECT_EQ(format::convert_graphic_type<unsigned long>().size(), sizeof(unsigned long));
        EXPECT_EQ(format::convert_graphic_type<long long>(), format::Int64);
        EXPECT_EQ(format::convert_graphic_type<unsigned long long>(), format::UnsignedInt64);
        EXPECT_EQ(format::convert_graphic_type<float>(), format::Float);
        EXPECT_EQ(format::convert_graphic_type<double>(), format::Float64);
        EXPECT_EQ(format::convert_graphic_type<void *>(), format::Undefined);
    }
    TEST(FormatTest, NormalizedRgbAndPacked32FormatsHaveExpectedByteSizes) {
        EXPECT_EQ(format::SignedNormalized8RGB.size(), 3u);
        EXPECT_EQ(format::UnsignedNormalized8ABGRPACK32.size(), 4u);
        EXPECT_EQ(format::Srgb8ABGRPACK32.size(), 4u);
    }
} // namespace

namespace {
    template<typename T>
    consteval bool value_conversion_matches_type() {
        return format::convert_graphic_type(T{}) == format::convert_graphic_type<T>();
    }
    static_assert(value_conversion_matches_type<char>());
    static_assert(value_conversion_matches_type<unsigned char>());
    static_assert(value_conversion_matches_type<short>());
    static_assert(value_conversion_matches_type<unsigned short>());
    static_assert(value_conversion_matches_type<int>());
    static_assert(value_conversion_matches_type<unsigned int>());
    static_assert(value_conversion_matches_type<long>());
    static_assert(value_conversion_matches_type<unsigned long>());
    static_assert(value_conversion_matches_type<long long>());
    static_assert(value_conversion_matches_type<unsigned long long>());
    static_assert(value_conversion_matches_type<float>());
    static_assert(value_conversion_matches_type<double>());
    static_assert(value_conversion_matches_type<StaticFormat>());
    static_assert(value_conversion_matches_type<void *>());
    static_assert(format::convert_graphic_type(DynamicFormat{}) == format::UnsignedInt);
    TEST(FormatTest, ValueConversionsPreserveBuiltinAndCustomFormats) {
        EXPECT_EQ(format::convert_graphic_type(1.0f), format::Float);
        EXPECT_EQ(format::convert_graphic_type(StaticFormat{}), format::FloatRG);
        EXPECT_EQ(format::convert_graphic_type(DynamicFormat{}), format::UnsignedInt);
        EXPECT_EQ(format::convert_graphic_type(nullptr), format::Undefined);
    }
} // namespace
