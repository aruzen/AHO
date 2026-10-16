#include <gtest/gtest.h>
#include <VSL/utils/string.hpp>
#include <VSL/utils/vsl_array.hpp>
#include "support/compile_probe.hpp"
namespace {
TEST(VslArrayTest, PointAndRectangleAccessorsReferenceOriginalFields) {
    vsl::__VSLD2PointDefault<float> point{3, 5};
    static_assert(vsl::is_point<decltype(point)>);
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
TEST(VslArrayTest, DISABLED_DefaultSizeAndRgbMeetTheirNamedConcepts) {
    EXPECT_TRUE((vsl::is_size<vsl::__VSLD2SizeDefault<float>>));
    EXPECT_TRUE((vsl::is_rgb<vsl::__VSLDRGBColorDefault<float>>));
    EXPECT_TRUE((vsl::is_rectangle<vsl::__VSLD2RectangleDefault<float>>));
}
TEST(VslArrayTest, DISABLED_ArraySizeCanBeInstantiatedForElementTypeAlias) {
    const auto result = vsl_test::compile_probe(R"cpp(
#include <VSL/utils/string.hpp>
#include <VSL/utils/vsl_array.hpp>
struct Pair {using element_type=float;float values[2];};
static_assert(vsl::vsl_array_size<Pair>()==2);
)cpp");
    EXPECT_EQ(result.status, 0) << result.diagnostics;
}
TEST(VslArrayTest, DISABLED_PointAdapterCanBeInstantiatedForDefaultPoint) {
    const auto result = vsl_test::compile_probe(R"cpp(
#include <VSL/utils/string.hpp>
#include <VSL/utils/vsl_array.hpp>
void probe() {
    vsl::__VSLD2PointDefault<float> point{3,5};
    vsl::__VSLD2Point<decltype(point)> adapter(&point);
}
)cpp");
    EXPECT_EQ(result.status, 0) << result.diagnostics;
}
} // namespace
