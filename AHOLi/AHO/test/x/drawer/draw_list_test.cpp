#include <AHO/x/drawer/draw_list.hpp>
#include <gtest/gtest.h>
namespace {
    using namespace aho::x;
    const aho::RGBA red(1, 0, 0, 1), green(0, 1, 0, 1);
    const aho::Triangle<float, vsl::D2> tri{aho::d2::PointF(0, 0), aho::d2::PointF(2, 0), aho::d2::PointF(0, 2)};
    TEST(DrawListTest, DefaultStateAndClearReuseCapacityAndResetTransform) {
        DrawList list;
        EXPECT_TRUE(list.empty());
        EXPECT_TRUE(list.vertices().empty());
        EXPECT_TRUE(list.batches().empty());
        list.reserve(128, 4);
        list.setTransform({.x = 4});
        list.triangle(tri, red);
        const auto* storage = list.vertices().data();
        list.clear();
        EXPECT_TRUE(list.empty());
        EXPECT_TRUE(list.batches().empty());
        EXPECT_EQ(list.transform().x, 0);
        list.triangle(tri, green);
        EXPECT_EQ(list.vertices().data(), storage);
        EXPECT_EQ(list.vertices()[0].position[0], 0);
        EXPECT_EQ(list.vertices()[0].color[1], 1);
    }
    TEST(DrawListTest, TriangleAndRectangleRetainSubmissionOrderInOneSolidBatch) {
        DrawList list;
        list.triangle(tri, red);
        list.rectangle({3, 4, 5, 6}, green);
        ASSERT_EQ(list.vertices().size(), 9u);
        ASSERT_EQ(list.batches().size(), 1u);
        EXPECT_EQ(list.batches()[0].vertexCount, 9u);
        EXPECT_EQ(list.batches()[0].firstVertex, 0u);
        EXPECT_EQ(list.vertices()[1].position, (std::array<float, 2>{2, 0}));
        EXPECT_EQ(list.vertices()[3].position, (std::array<float, 2>{3, 4}));
        EXPECT_EQ(list.vertices()[5].position, (std::array<float, 2>{8, 10}));
        EXPECT_EQ(list.vertices()[0].color, (std::array<float, 4>{1, 0, 0, 1}));
        EXPECT_EQ(list.vertices()[8].color, (std::array<float, 4>{0, 1, 0, 1}));
    }
    TEST(DrawListTest, TextureChangesSplitBatchesWithoutSortingOrReordering) {
        DrawList list;
        list.rectangle({0, 0, 1, 1}, red, {7}, {.x = .25f, .y = .5f, .width = .25f, .height = .5f});
        list.rectangle({1, 0, 1, 1}, green, {7});
        list.triangle(tri, red);
        list.rectangle({2, 0, 1, 1}, red, {7});
        ASSERT_EQ(list.batches().size(), 3u);
        EXPECT_EQ(list.batches()[0].texture.value, 7u);
        EXPECT_EQ(list.batches()[0].vertexCount, 12u);
        EXPECT_EQ(list.batches()[1].texture.value, 0u);
        EXPECT_EQ(list.batches()[1].firstVertex, 12u);
        EXPECT_EQ(list.batches()[2].firstVertex, 15u);
        EXPECT_EQ(list.vertices()[2].uv, (std::array<float, 2>{.5f, 1}));
    }
    TEST(DrawListTest, TransformAffectsOnlySubsequentGeometryAndSupportsGenericTriangleViews) {
        DrawList list;
        list.triangle(tri, red);
        list.setTransform({.xx = 0, .xy = -2, .yx = 2, .yy = 0, .x = 10, .y = 20});
        list.geometry(tri, green);
        EXPECT_EQ(list.vertices()[0].position, (std::array<float, 2>{0, 0}));
        EXPECT_EQ(list.vertices()[4].position, (std::array<float, 2>{10, 24}));
        EXPECT_EQ(list.vertices()[5].position, (std::array<float, 2>{6, 20}));
        auto copy = list;
        copy.clear();
        EXPECT_EQ(list.vertices().size(), 6u);
    }
    TEST(DrawListTest, InvalidInputAndOverflowLeaveExistingGeometryUnchanged) {
        DrawList list;
        list.triangle(tri, red);
        EXPECT_THROW(list.rectangle({0, 0, INFINITY, 1}, red), std::invalid_argument);
        EXPECT_THROW(list.triangle(tri, aho::RGBA(NAN, 0, 0, 1)), std::invalid_argument);
        EXPECT_THROW(list.setTransform({.xx = INFINITY}), std::invalid_argument);
        EXPECT_EQ(list.transform().xx, 1);
        EXPECT_EQ(list.vertices().size(), 3u);
        EXPECT_EQ(list.batches()[0].vertexCount, 3u);
        if constexpr (sizeof(std::size_t) > sizeof(std::uint32_t))
            EXPECT_THROW(list.reserve(std::size_t{UINT32_MAX} + 1), std::length_error);
    }
} // namespace
