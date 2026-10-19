#include <VSL/utils/vsl_array.hpp>
#include <gtest/gtest.h>
#include <array>
#include <type_traits>

namespace {
    struct InvalidIndexMetadata {
        inline static const char* point_index = "point";
        inline static const char* size_index = "size";
        inline static const char* color_index = "color";
    };
    template<class T, std::size_t N>
    struct Packed : std::array<T, N> {
        using element_type = T;
    };
    template<class T>
    struct IndexedPacked : Packed<T, 8> {
        static constexpr std::size_t point_index = 3, size_index = 1, color_index = 4;
    };
    template<class T>
    struct PointIndexedPacked : Packed<T, 8> {
        static constexpr std::size_t point_index = 4;
    };
    template<class T>
    struct SizeIndexedPacked : Packed<T, 8> {
        static constexpr std::size_t size_index = 5;
    };
    template<class T>
    struct RuntimeIndexedPacked : Packed<T, 8> {
        inline static std::size_t point_index = 1, size_index = 3, color_index = 5;
    };
    template<class P>
    P packed_values() {
        static_assert(std::is_standard_layout_v<P>);
        P result{};
        for (std::size_t i = 0; i < result.size(); ++i)
            result[i] = static_cast<typename P::element_type>(i + 1);
        return result;
    }
    template<class T>
    class VslArrayTest : public testing::Test {};
    using ArrayScalars = testing::Types<int, float, double>;
    TYPED_TEST_SUITE(VslArrayTest, ArrayScalars);
    TYPED_TEST(VslArrayTest, ElementTypeAliasesProduceTheExpectedArraySize) {
        using P = Packed<TypeParam, 8>;
        static_assert(vsl::vsl_array_size<P>() == 8);
        static_assert(vsl::convert_to_vsl_sized_array<P, 8>);
        static_assert(vsl::convert_to_vsl_at_least_array<P, 4>);
        EXPECT_EQ(vsl::vsl_array_size<P>(), 8u);
    }
    TYPED_TEST(VslArrayTest, NamedPointSizeAndRgbFieldsBindToTheirOriginalValues) {
        using T = TypeParam;
        vsl::__VSLD2PointDefault<T> point{T{1}, T{2}};
        vsl::__VSLD2SizeDefault<T> size{T{3}, T{4}};
        vsl::__VSLDRGBColorDefault<T> color{T{5}, T{6}, T{7}};
        static_assert(vsl::is_point<decltype(point)>);
        static_assert(vsl::is_size<decltype(size)>);
        static_assert(vsl::is_rgb<decltype(color)>);
        vsl::__VSLD2Point<decltype(point)> point_view(&point);
        vsl::__VSLD2Size<decltype(size)> size_view(&size);
        vsl::__VSLDRGBColor<decltype(color)> color_view(&color);
        EXPECT_EQ(&point_view.x, &point.x);
        EXPECT_EQ(&point_view.y, &point.y);
        EXPECT_EQ(&size_view.width, &size.width);
        EXPECT_EQ(&size_view.height, &size.height);
        EXPECT_EQ(&color_view.r, &color.r);
        EXPECT_EQ(&color_view.g, &color.g);
        EXPECT_EQ(&color_view.b, &color.b);
        point_view.x = T{8};
        size_view.height = T{9};
        color_view.b = T{10};
        EXPECT_EQ(point.x, T{8});
        EXPECT_EQ(size.height, T{9});
        EXPECT_EQ(color.b, T{10});
    }
    TYPED_TEST(VslArrayTest, NamedRectanglesSelectTheirFieldAdapterWithoutAmbiguity) {
        using T = TypeParam;
        vsl::__VSLD2RectangleDefault<T> rectangle{{T{1}, T{2}}, {T{3}, T{4}}};
        static_assert(vsl::is_rectangle<decltype(rectangle)>);
        vsl::__VSLD2Rectangle<decltype(rectangle)> view(&rectangle);
        EXPECT_EQ(&view.x, &rectangle.x);
        EXPECT_EQ(&view.y, &rectangle.y);
        EXPECT_EQ(&view.width, &rectangle.width);
        EXPECT_EQ(&view.height, &rectangle.height);
        view.width = T{9};
        EXPECT_EQ(rectangle.width, T{9});
    }
    TYPED_TEST(VslArrayTest, PackedPointsUseTheFirstTwoElements) {
        auto values = packed_values<Packed<TypeParam, 8>>();
        vsl::__VSLD2Point<decltype(values)> view(&values);
        EXPECT_EQ(&view.x, &values[0]);
        EXPECT_EQ(&view.y, &values[1]);
        view.y = TypeParam{9};
        EXPECT_EQ(values[1], TypeParam{9});
    }
    TYPED_TEST(VslArrayTest, PackedSizesInitializeStorageBeforeBindingReferences) {
        auto values = packed_values<Packed<TypeParam, 8>>();
        vsl::__VSLD2Size<decltype(values)> view(&values);
        EXPECT_EQ(&view.width, &values[0]);
        EXPECT_EQ(&view.height, &values[1]);
        view.height = TypeParam{9};
        EXPECT_EQ(values[1], TypeParam{9});
    }
    TYPED_TEST(VslArrayTest, PackedColorsInitializeStorageBeforeBindingReferences) {
        auto values = packed_values<Packed<TypeParam, 8>>();
        vsl::__VSLDRGBColor<decltype(values)> view(&values);
        EXPECT_EQ(&view.r, &values[0]);
        EXPECT_EQ(&view.g, &values[1]);
        EXPECT_EQ(&view.b, &values[2]);
        view.b = TypeParam{9};
        EXPECT_EQ(values[2], TypeParam{9});
    }
    TYPED_TEST(VslArrayTest, PackedRectanglesUseDefaultOffsetsWithoutIndexMembers) {
        auto values = packed_values<Packed<TypeParam, 8>>();
        vsl::__VSLD2Rectangle<decltype(values)> view(&values);
        EXPECT_EQ(&view.x, &values[0]);
        EXPECT_EQ(&view.y, &values[1]);
        EXPECT_EQ(&view.width, &values[2]);
        EXPECT_EQ(&view.height, &values[3]);
        view.height = TypeParam{9};
        EXPECT_EQ(values[3], TypeParam{9});
    }
    TYPED_TEST(VslArrayTest, ExplicitStaticIndicesSelectTheRequestedElements) {
        auto values = packed_values<IndexedPacked<TypeParam>>();
        static_assert(vsl::has_point_index<decltype(values)>);
        static_assert(vsl::has_size_index<decltype(values)>);
        static_assert(vsl::has_rgb_color_index<decltype(values)>);
        vsl::__VSLD2Point<decltype(values)> point(&values);
        vsl::__VSLD2Size<decltype(values)> size(&values);
        vsl::__VSLDRGBColor<decltype(values)> color(&values);
        vsl::__VSLD2Rectangle<decltype(values)> rectangle(&values);
        EXPECT_EQ(&point.x, &values[3]);
        EXPECT_EQ(&point.y, &values[4]);
        EXPECT_EQ(&size.width, &values[1]);
        EXPECT_EQ(&size.height, &values[2]);
        EXPECT_EQ(&color.r, &values[4]);
        EXPECT_EQ(&color.g, &values[5]);
        EXPECT_EQ(&color.b, &values[6]);
        EXPECT_EQ(&rectangle.x, &values[3]);
        EXPECT_EQ(&rectangle.y, &values[4]);
        EXPECT_EQ(&rectangle.width, &values[1]);
        EXPECT_EQ(&rectangle.height, &values[2]);
    }
    TYPED_TEST(VslArrayTest, EachRectangleIndexCanBeOmittedIndependently) {
        auto point_values = packed_values<PointIndexedPacked<TypeParam>>();
        auto size_values = packed_values<SizeIndexedPacked<TypeParam>>();
        vsl::__VSLD2Rectangle<decltype(point_values)> point_indexed(&point_values);
        vsl::__VSLD2Rectangle<decltype(size_values)> size_indexed(&size_values);
        EXPECT_EQ(&point_indexed.x, &point_values[4]);
        EXPECT_EQ(&point_indexed.y, &point_values[5]);
        EXPECT_EQ(&point_indexed.width, &point_values[2]);
        EXPECT_EQ(&point_indexed.height, &point_values[3]);
        EXPECT_EQ(&size_indexed.x, &size_values[0]);
        EXPECT_EQ(&size_indexed.y, &size_values[1]);
        EXPECT_EQ(&size_indexed.width, &size_values[5]);
        EXPECT_EQ(&size_indexed.height, &size_values[6]);
    }
    TYPED_TEST(VslArrayTest, StaticRuntimeIndicesAreAcceptedAlongsideConstantIndices) {
        auto values = packed_values<RuntimeIndexedPacked<TypeParam>>();
        static_assert(vsl::has_point_index<decltype(values)>);
        static_assert(vsl::has_size_index<decltype(values)>);
        static_assert(vsl::has_rgb_color_index<decltype(values)>);
        vsl::__VSLD2Point<decltype(values)> point(&values);
        vsl::__VSLD2Size<decltype(values)> size(&values);
        vsl::__VSLDRGBColor<decltype(values)> color(&values);
        vsl::__VSLD2Rectangle<decltype(values)> rectangle(&values);
        EXPECT_EQ(&point.x, &values[1]);
        EXPECT_EQ(&size.width, &values[3]);
        EXPECT_EQ(&color.r, &values[5]);
        EXPECT_EQ(&rectangle.x, &values[1]);
        EXPECT_EQ(&rectangle.width, &values[3]);
    }
    TYPED_TEST(VslArrayTest, ReferenceConvertibleNamedFieldsKeepTheirReferenceSemantics) {
        using T = TypeParam;
        struct Proxy {
            T* value;
            operator T&() const { return *value; }
        };
        struct Fields {
            using element_type = T;
            Proxy x, y, width, height, r, g, b;
        };
        std::array<T, 7> values{1, 2, 3, 4, 5, 6, 7};
        Fields fields{{&values[0]}, {&values[1]}, {&values[2]}, {&values[3]}, {&values[4]}, {&values[5]}, {&values[6]}};
        static_assert(vsl::is_point<Fields> && vsl::is_size<Fields> && vsl::is_rgb<Fields>);
        vsl::__VSLD2Point<Fields> point(&fields);
        vsl::__VSLD2Size<Fields> size(&fields);
        vsl::__VSLDRGBColor<Fields> color(&fields);
        vsl::__VSLD2Rectangle<Fields> rectangle(&fields);
        EXPECT_EQ(&point.x, &values[0]);
        EXPECT_EQ(&size.width, &values[2]);
        EXPECT_EQ(&color.b, &values[6]);
        EXPECT_EQ(&rectangle.height, &values[3]);
        point.x = T{9};
        size.width = T{10};
        color.b = T{11};
        rectangle.height = T{12};
        EXPECT_EQ(values[0], T{9});
        EXPECT_EQ(values[2], T{10});
        EXPECT_EQ(values[6], T{11});
        EXPECT_EQ(values[3], T{12});
    }
    TEST(VslArrayTest, PointAndRectangleAccessorsReferenceOriginalFields) {
        vsl::__VSLD2PointDefault<float> point{3, 5};
        vsl::__VSLD2PointAccessor<float> adapter{point.x, point.y};
        EXPECT_EQ(adapter.x, 3);
        EXPECT_EQ(adapter.y, 5);
        adapter.x = 7;
        adapter.y = 11;
        EXPECT_EQ(point.x, 7);
        EXPECT_EQ(point.y, 11);
        float width = 13, height = 17;
        vsl::__VSLD2RectangleAccessor<float> rectangle{{point.x, point.y}, {width, height}};
        rectangle.width = 19;
        EXPECT_EQ(width, 19);
        EXPECT_EQ(rectangle.height, 17);
    }
    TEST(VslArrayConceptTest, NamedFieldsMustBindToTheDeclaredElementReferenceType) {
        struct WrongFields {
            using element_type = float;
            int x, y, width, height, r, g, b;
        };
        struct ReadOnlyFields {
            using element_type = float;
            const float x = 0, y = 0, width = 0, height = 0, r = 0, g = 0, b = 0;
        };
        static_assert(!vsl::is_point<WrongFields> && !vsl::is_size<WrongFields> && !vsl::is_rgb<WrongFields>);
        static_assert(!vsl::is_point<ReadOnlyFields> && !vsl::is_size<ReadOnlyFields> && !vsl::is_rgb<ReadOnlyFields>);
        EXPECT_FALSE(vsl::is_rectangle<WrongFields>);
    }
    TEST(VslArrayConceptTest, NonstaticOrNonNumericIndexFieldsAreNotStaticArrayMetadata) {
        struct Nonstatic {
            std::size_t point_index, size_index, color_index;
        };
        using Invalid = InvalidIndexMetadata;
        static_assert(!vsl::has_point_index<Nonstatic> && !vsl::has_size_index<Nonstatic> && !vsl::has_rgb_color_index<Nonstatic>);
        EXPECT_FALSE(vsl::has_point_index<Invalid>);
        EXPECT_FALSE(vsl::has_size_index<Invalid>);
        EXPECT_FALSE(vsl::has_rgb_color_index<Invalid>);
    }
} // namespace
