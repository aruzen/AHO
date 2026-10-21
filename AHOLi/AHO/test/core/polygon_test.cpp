#include <AHO/core/polygon.hpp>
#include <gtest/gtest.h>
#include <array>
#include <cstdint>
#include <cmath>
#include <limits>

namespace {
    using P = aho::d2::PointD;
    using Mesh = aho::Polygon<double, vsl::D2>;
    using I = aho::IndexedTriangle<std::size_t>;
    static_assert(aho::concepts::is_points_object<Mesh>);
    static_assert(aho::concepts::is_lines_object<Mesh>);
    static_assert(aho::concepts::is_triangles_object<Mesh>);
    static_assert(std::same_as<decltype(std::declval<const Mesh&>().points()[0]), const P&>);

    TEST(PolygonTest, DefaultMeshIsEmpty) {
        const Mesh mesh;
        EXPECT_TRUE(mesh.empty());
        EXPECT_EQ(mesh.size(), 0u);
        EXPECT_TRUE(mesh.points().empty());
        EXPECT_TRUE(mesh.indices().empty());
        EXPECT_EQ(std::ranges::distance(mesh.triangles()), 0);
        EXPECT_EQ(std::ranges::distance(mesh.lines()), 0);
        EXPECT_EQ(mesh.area(), 0);
    }
    TEST(PolygonTest, OwnsVerticesAndIndicesAndExposesGeometryViews) {
        std::array<P, 4> points{P(0, 0), P(2, 0), P(2, 3), P(0, 3)};
        std::array<I, 2> indices{I{{0}, {1}, {2}}, I{{0}, {2}, {3}}};
        const Mesh mesh(points, indices);
        points[0] = P(99, 99);
        indices[0].idx1.index = 99;
        EXPECT_FALSE(mesh.empty());
        EXPECT_EQ(mesh.size(), 2u);
        EXPECT_EQ(mesh.points()[0].value.x, 0);
        EXPECT_EQ(mesh.indices()[0].idx1.index, 0u);
        EXPECT_EQ(std::ranges::distance(mesh.triangles()), 2);
        EXPECT_EQ(std::ranges::distance(mesh.lines()), 6);
        EXPECT_DOUBLE_EQ(mesh.triangles().front().area(), 3);
        EXPECT_DOUBLE_EQ(mesh.lines().front().length(), 2);
        EXPECT_DOUBLE_EQ(mesh.area(), 6);
        auto copy = mesh;
        auto moved = std::move(copy);
        EXPECT_DOUBLE_EQ(moved.area(), 6);
        EXPECT_DOUBLE_EQ(mesh.area(), 6);
    }
    TEST(PolygonTest, InvalidIndexIsRejectedForEveryTriangleCorner) {
        const std::array<P, 3> points{P(0, 0), P(1, 0), P(0, 1)};
        for (const auto triangle : {I{{3}, {1}, {2}}, I{{0}, {3}, {2}}, I{{0}, {1}, {3}}}) {
            const std::array indices{triangle};
            EXPECT_THROW(Mesh(points, indices), std::out_of_range);
        }
    }
    TEST(PolygonTest, SupportsThreeDimensionsAndNarrowIndices) {
        using Mesh3 = aho::Polygon<double, vsl::D3, aho::coordinate::DefaultCoordinateInfo<vsl::D3>::Value, std::uint16_t>;
        const std::array<aho::d3::PointD, 3> points{aho::d3::PointD(0, 0, 0), aho::d3::PointD(0, 3, 0), aho::d3::PointD(0, 0, 4)};
        const std::array indices{aho::IndexedTriangle<std::uint16_t>{{0}, {1}, {2}}};
        const Mesh3 mesh(points, indices);
        EXPECT_DOUBLE_EQ(mesh.area(), 6);
        EXPECT_EQ(mesh.indices()[0].idx3.index, 2);
        EXPECT_EQ(std::ranges::distance(aho::views::triangles(mesh)), 1);
    }
    TEST(PolygonTest, IntegerAreaWidensBeforeSubtractionAndMultiplication) {
        const std::array<aho::d2::PointI, 3> points{aho::d2::PointI(-100000, -100000), aho::d2::PointI(100000, -100000), aho::d2::PointI(-100000, 100000)};
        const std::array indices{I{{0}, {1}, {2}}};
        const aho::Polygon<int, vsl::D2> mesh(points, indices);
        EXPECT_DOUBLE_EQ(mesh.area(), 20000000000.0);

        using IntPoint = aho::Point_<std::int32_t, vsl::D2>;
        constexpr auto lowest = std::numeric_limits<std::int32_t>::min();
        constexpr auto highest = std::numeric_limits<std::int32_t>::max();
        const std::array extreme_points{IntPoint(lowest, lowest), IntPoint(highest, lowest), IntPoint(lowest, highest)};
        const aho::Polygon<std::int32_t, vsl::D2> extreme_mesh(extreme_points, indices);
        EXPECT_DOUBLE_EQ(extreme_mesh.area(), 9223372032559808512.0);
    }

    TEST(PolygonTest, ThreeDimensionalAreaPreservesLargeAndSmallNonAxisAlignedTriangles) {
        const std::array indices{I{{0}, {1}, {2}}};
        for (const double scale : {1e-150, 1e150}) {
            const std::array points{aho::d3::PointD(0, 0, 0), aho::d3::PointD(scale, scale, 0), aho::d3::PointD(0, scale, scale)};
            const aho::Polygon<double, vsl::D3> mesh(points, indices);
            EXPECT_NEAR(mesh.area() / scale / scale, std::sqrt(3.0) / 2, 1e-12);
        }
    }
    TEST(PolygonTest, RejectsVertexCountsThatCannotBeIndexed) {
        using Small = aho::Polygon<double, vsl::D2, aho::coordinate::DefaultCoordinateInfo<vsl::D2>::Value, std::uint8_t>;
        const std::vector<P> points(257);
        EXPECT_THROW(Small(points, {}), std::length_error);
        const std::vector<P> maximum(256);
        EXPECT_NO_THROW(Small(maximum, {}));
    }
} // namespace
