#include <gtest/gtest.h>
#include <VSL/format.hpp>
#include <array>

namespace {
    namespace format = vsl::data_format;
    struct Family {
        std::array<format::Format, 4> vectors;
        std::uint32_t component_bytes;
        const char* name;
    };
#define FAMILY(name, bytes)                                                                          \
    Family {                                                                                         \
        {format::name##_R, format::name##_RG, format::name##_RGB, format::name##_RGBA}, bytes, #name \
    }
    const std::array FAMILIES{FAMILY(UNSIGNED_NORMALIZED8, 1),
                              FAMILY(SIGNED_NORMALIZED8, 1),
                              FAMILY(UNSIGNED_SCALED8, 1),
                              FAMILY(SIGNED_SCALED8, 1),
                              FAMILY(UNSIGNED_INT8, 1),
                              FAMILY(SIGNED_INT8, 1),
                              FAMILY(SRGB8, 1),
                              FAMILY(UNSIGNED_NORMALIZED16, 2),
                              FAMILY(SIGNED_NORMALIZED16, 2),
                              FAMILY(UNSIGNED_SCALED16, 2),
                              FAMILY(SIGNED_SCALED16, 2),
                              FAMILY(UNSIGNED_INT16, 2),
                              FAMILY(SIGNED_INT16, 2),
                              FAMILY(SIGNED_FLOAT16, 2),
                              FAMILY(UNSIGNED_INT32, 4),
                              FAMILY(SIGNED_INT32, 4),
                              FAMILY(SIGNED_FLOAT32, 4),
                              FAMILY(UNSIGNED_INT64, 8),
                              FAMILY(SIGNED_INT64, 8),
                              FAMILY(SIGNED_FLOAT64, 8)};
#undef FAMILY
    class FormatFamilyTest : public testing::TestWithParam<Family> {};
    TEST_P(FormatFamilyTest, ConvertsBetweenScalarAndVectorTypes) {
        const auto& family = GetParam();
        for (std::size_t source = 0; source < 4; ++source) {
            const auto value = family.vectors[source];
            SCOPED_TRACE(source);
            EXPECT_EQ(value.get_vec_length(), source + 1);
            EXPECT_EQ(format::convert(value.type()), value);
            EXPECT_EQ(value.to_vec<1>().type(), family.vectors[0].type());
            EXPECT_EQ(value.to_vec<2>().type(), family.vectors[1].type());
            EXPECT_EQ(value.to_vec<3>().type(), family.vectors[2].type());
            EXPECT_EQ(value.to_vec<4>().type(), family.vectors[3].type());
            for (std::size_t target = 1; target <= 4; ++target)
                EXPECT_EQ(value.to_vec(target).type(), family.vectors[target - 1].type());
        }
    }
    TEST_P(FormatFamilyTest, ByteSizeMatchesComponentCountAndWidth) {
        const auto& family = GetParam();
        for (std::size_t i = 0; i < 4; ++i) {
            EXPECT_EQ(family.vectors[i].size(), family.component_bytes * (i + 1));
        }
    }
    INSTANTIATE_TEST_SUITE_P(StandardFormats, FormatFamilyTest, testing::ValuesIn(FAMILIES),
                             [](const testing::TestParamInfo<Family>& info) { return info.param.name; });
    TEST(FormatTest, UndefinedAndUnsupportedLengthsHaveNoStorage) {
        EXPECT_EQ(format::UNDEFINED.size(), 0u);
        EXPECT_EQ(format::UNDEFINED.get_vec_length(), 0u);
        EXPECT_EQ(format::UNDEFINED.to_vec<4>(), format::UNDEFINED);
        EXPECT_EQ(format::FLOAT.to_vec(0), format::UNDEFINED);
        EXPECT_EQ(format::FLOAT.to_vec(5), format::UNDEFINED);
        EXPECT_EQ(format::FLOAT.to_vec<0>(), format::UNDEFINED);
        EXPECT_EQ(format::FLOAT.to_vec<5>(), format::UNDEFINED);
        EXPECT_EQ(format::convert(static_cast<format::FormatTypes>(0xffffffffu)), format::UNDEFINED);
    }
    TEST(FormatTest, ConstructionComparisonAndCommonAliasesPreserveMeaning) {
        const format::Format a(format::FormatTypes::SIGNED_FLOAT32_RG, 8);
        const format::Format b(format::FormatTypes::SIGNED_FLOAT32_RG, 8);
        EXPECT_EQ(a, b);
        EXPECT_EQ(a.type(), format::FormatTypes::SIGNED_FLOAT32_RG);
        EXPECT_EQ(a.size(), 8u);
        EXPECT_NE(a, format::FLOAT_RGB);
        EXPECT_TRUE((a <=> b) == 0);
        EXPECT_EQ(format::FLOAT, format::SIGNED_FLOAT32_R);
        EXPECT_EQ(format::INT_VEC4, format::SIGNED_INT32_RGBA);
        EXPECT_EQ(format::UNSIGNED_INT, format::UNSIGNED_INT32_R);
    }
    struct StaticFormat {
        static constexpr auto GRAPHIC_TYPE = format::FLOAT_RG;
    };
    struct DynamicFormat {
        consteval auto get_graphic_type() const {
            return format::UNSIGNED_INT;
        }
    };
    static_assert(vsl::concepts::is_static_convertible_graphic_type<StaticFormat>);
    static_assert(!vsl::concepts::is_static_convertible_graphic_type<DynamicFormat>);
    static_assert(vsl::concepts::is_convertible_graphic_type<DynamicFormat>);
    static_assert(!vsl::concepts::is_convertible_graphic_type<void*>);
    static_assert(format::convert_graphic_type<StaticFormat>() == format::FLOAT_RG);
    static_assert(format::convert_graphic_type(DynamicFormat{}) == format::UNSIGNED_INT);
    TEST(FormatTest, BuiltinTypesMapToStorageOfTheirWidth) {
        EXPECT_EQ(format::convert_graphic_type<char>(), format::INT8);
        EXPECT_EQ(format::convert_graphic_type<unsigned char>(), format::UNSIGNED_INT8);
        EXPECT_EQ(format::convert_graphic_type<short>(), format::INT16);
        EXPECT_EQ(format::convert_graphic_type<unsigned short>(), format::UNSIGNED_INT16);
        EXPECT_EQ(format::convert_graphic_type<int>(), format::INT);
        EXPECT_EQ(format::convert_graphic_type<unsigned int>(), format::UNSIGNED_INT);
        EXPECT_EQ(format::convert_graphic_type<long>().size(), sizeof(long));
        EXPECT_EQ(format::convert_graphic_type<unsigned long>().size(), sizeof(unsigned long));
        EXPECT_EQ(format::convert_graphic_type<long long>(), format::INT64);
        EXPECT_EQ(format::convert_graphic_type<unsigned long long>(), format::UNSIGNED_INT64);
        EXPECT_EQ(format::convert_graphic_type<float>(), format::FLOAT);
        EXPECT_EQ(format::convert_graphic_type<double>(), format::FLOAT64);
        EXPECT_EQ(format::convert_graphic_type<void*>(), format::UNDEFINED);
    }
    TEST(FormatTest, NormalizedRgbAndPacked32FormatsHaveExpectedByteSizes) {
        EXPECT_EQ(format::SIGNED_NORMALIZED8_RGB.size(), 3u);
        EXPECT_EQ(format::UNSIGNED_NORMALIZED8_ABGRPAC_K32.size(), 4u);
        EXPECT_EQ(format::SRGB8_ABGRPAC_K32.size(), 4u);
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
    static_assert(value_conversion_matches_type<void*>());
    static_assert(format::convert_graphic_type(DynamicFormat{}) == format::UNSIGNED_INT);
    TEST(FormatTest, ValueConversionsPreserveBuiltinAndCustomFormats) {
        EXPECT_EQ(format::convert_graphic_type(1.0f), format::FLOAT);
        EXPECT_EQ(format::convert_graphic_type(StaticFormat{}), format::FLOAT_RG);
        EXPECT_EQ(format::convert_graphic_type(DynamicFormat{}), format::UNSIGNED_INT);
        EXPECT_EQ(format::convert_graphic_type(nullptr), format::UNDEFINED);
    }
} // namespace
