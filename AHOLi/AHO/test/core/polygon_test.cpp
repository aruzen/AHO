#include <AHO/core/polygon.hpp>
#include <gtest/gtest.h>
#include <array>
#include <cstdint>
#include <cmath>
#include <limits>
#include <unordered_map>

namespace {
    using P = aho::d2::PointD;
    using Mesh = aho::Polygon<double, vsl::D2>;
    using I = aho::IndexedTriangle<std::size_t>;
    static_assert(aho::concepts::is_points_object<Mesh>);
    static_assert(aho::concepts::is_lines_object<Mesh>);
    static_assert(aho::concepts::is_triangles_object<Mesh>);
    static_assert(std::same_as<decltype(std::declval<const Mesh&>().points()[0]), const P&>);

    template<typename T>
    class IntegerPolygonAreaTest : public testing::Test {};
    using XZ = aho::coordinate::MakeCoordinateInfo<true, false, true>::Value;
    using YZ = aho::coordinate::MakeCoordinateInfo<false, true, true>::Value;
    using IntegerAreaPoints = testing::Types<
        aho::d2::Point<std::int32_t>, aho::d2::Point<std::int32_t, XZ>, aho::d2::Point<std::int32_t, YZ>, aho::d3::Point<std::int32_t>,
        aho::d2::Point<std::uint32_t>, aho::d2::Point<std::uint32_t, XZ>, aho::d2::Point<std::uint32_t, YZ>, aho::d3::Point<std::uint32_t>>;

    struct PolygonAllocationState {
        inline static std::size_t NEXT_ID = 0;
        inline static bool FAIL_NEXT = false;
        inline static int CONSTRUCT_BEFORE_FAILURE = -1;
        inline static std::unordered_map<void*, std::size_t> OWNERS;
    };

    template<typename T>
    class PolygonAllocator {
        std::size_t _id = ++PolygonAllocationState::NEXT_ID;

    public:
        using value_type = T;
        PolygonAllocator() = default;
        template<typename U>
        PolygonAllocator(const PolygonAllocator<U>& other) noexcept;
        [[nodiscard]] T* allocate(std::size_t count);
        void deallocate(T* data, std::size_t count) noexcept;
        template<typename U, typename... Args>
        void construct(U* data, Args&&... args);
        [[nodiscard]] std::size_t id() const noexcept;
        [[nodiscard]] PolygonAllocator select_on_container_copy_construction() const;
        bool operator==(const PolygonAllocator&) const noexcept = default;
    };

    template<typename T>
    template<typename U>
    PolygonAllocator<T>::PolygonAllocator(const PolygonAllocator<U>& other) noexcept : _id(other.id()) {}

    template<typename T>
    T* PolygonAllocator<T>::allocate(std::size_t count) {
        if (std::exchange(PolygonAllocationState::FAIL_NEXT, false))
            throw std::bad_alloc();
        auto* result = std::allocator<T>{}.allocate(count);
        PolygonAllocationState::OWNERS.emplace(result, _id);
        return result;
    }

    template<typename T>
    void PolygonAllocator<T>::deallocate(T* data, std::size_t count) noexcept {
        const auto found = PolygonAllocationState::OWNERS.find(data);
        EXPECT_NE(found, PolygonAllocationState::OWNERS.end());
        if (found != PolygonAllocationState::OWNERS.end()) {
            EXPECT_EQ(found->second, _id);
            PolygonAllocationState::OWNERS.erase(found);
        }
        std::allocator<T>{}.deallocate(data, count);
    }

    template<typename T>
    template<typename U, typename... Args>
    void PolygonAllocator<T>::construct(U* data, Args&&... args) {
        if (PolygonAllocationState::CONSTRUCT_BEFORE_FAILURE == 0) {
            PolygonAllocationState::CONSTRUCT_BEFORE_FAILURE = -1;
            throw std::runtime_error("allocator construction");
        }
        if (PolygonAllocationState::CONSTRUCT_BEFORE_FAILURE > 0)
            --PolygonAllocationState::CONSTRUCT_BEFORE_FAILURE;
        std::construct_at(data, std::forward<Args>(args)...);
    }

    template<typename T>
    std::size_t PolygonAllocator<T>::id() const noexcept {
        return _id;
    }

    template<typename T>
    auto PolygonAllocator<T>::select_on_container_copy_construction() const -> PolygonAllocator {
        return {};
    }

    TYPED_TEST_SUITE(IntegerPolygonAreaTest, IntegerAreaPoints);
    TYPED_TEST(IntegerPolygonAreaTest, UnitDeterminantSurvivesEveryVertexOrderAndAreaAccumulation) {
        using Polygon = aho::Polygon<typename TypeParam::ElementType, typename TypeParam::Dimension, typename TypeParam::CoordinateInfo>;
        using Point = typename Polygon::PointType;
        using R = typename Point::ElementType;
        auto point = [](R first, R second) {
            if constexpr (Point::Dimension::DIMENSION == 2)
                return Point(first, second);
            else
                return Point(first, second, first - second);
        };
        const auto high = std::numeric_limits<R>::max();
        const std::array points{point(0, 0), point(high - 1, high - 2), point(high, high - 1)};
        const auto expected = Point::Dimension::DIMENSION == 2 ? .5 : std::sqrt(3.) / 2;
        std::array<std::size_t, 3> order{0, 1, 2};
        do {
            const I triangle{{order[0]}, {order[1]}, {order[2]}};
            const Polygon one(points, std::array{triangle});
            EXPECT_DOUBLE_EQ(one.area(), expected);
            const Polygon two(points, std::array{triangle, triangle});
            EXPECT_DOUBLE_EQ(two.area(), 2 * expected);
        } while (std::next_permutation(order.begin(), order.end()));
    }

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

    TEST(PolygonTest, SpanContractAndContiguousGeometrySurviveCopyMoveAndAssignment) {
        static_assert(std::same_as<decltype(std::declval<const Mesh&>().points()), std::span<const P>>);
        static_assert(std::same_as<decltype(std::declval<const Mesh&>().indices()), std::span<const I>>);
        static_assert(std::is_nothrow_move_constructible_v<Mesh>);
        static_assert(std::is_nothrow_move_assignable_v<Mesh>);
        const std::array points{P(0, 0), P(2, 0), P(0, 3)};
        const std::array indices{I{{0}, {1}, {2}}};
        Mesh source(points, indices), copy;
        copy = source;
        EXPECT_NE(copy.points().data(), source.points().data());
        EXPECT_NE(copy.indices().data(), source.indices().data());
        EXPECT_DOUBLE_EQ(copy.area(), 3);
        const auto* vertex_data = copy.points().data();
        const auto* index_data = copy.indices().data();
        Mesh moved;
        moved = std::move(copy);
        EXPECT_EQ(moved.points().data(), vertex_data);
        EXPECT_EQ(moved.indices().data(), index_data);
        EXPECT_TRUE(copy.points().empty());
        EXPECT_TRUE(copy.indices().empty());
        EXPECT_EQ(&moved.points()[1], moved.points().data() + 1);
        EXPECT_EQ(moved.indices()[0].idx3.index, 2u);
        auto& self = moved;
        moved = self;
        moved = std::move(self);
        EXPECT_DOUBLE_EQ(moved.area(), 3);
    }

    TEST(PolygonTest, StatefulAllocatorKeepsItsOwnAllocationsDuringAssignment) {
        using Allocated = aho::Polygon<double, vsl::D2,
                                       aho::coordinate::DefaultCoordinateInfo<vsl::D2>::Value, std::size_t, PolygonAllocator<I>>;
        static_assert(!std::is_nothrow_move_assignable_v<Allocated>);
        const std::array points{P(0, 0), P(2, 0), P(0, 3)};
        const std::array indices{I{{0}, {1}, {2}}, I{{0}, {2}, {1}}};
        {
            Allocated source(points, indices), destination(points, std::span{indices}.first(1));
            destination = source;
            EXPECT_EQ(destination.size(), 2u);
            EXPECT_DOUBLE_EQ(destination.area(), 6);
            Allocated moved(points, std::span{indices}.first(1));
            moved = std::move(destination);
            EXPECT_EQ(moved.size(), 2u);
            EXPECT_DOUBLE_EQ(moved.area(), 6);
            EXPECT_TRUE(destination.points().empty());
            EXPECT_TRUE(destination.indices().empty());
            auto copied = source;
            EXPECT_EQ(copied.size(), 2u);
            auto constructed = std::move(copied);
            EXPECT_TRUE(copied.indices().empty());
            EXPECT_DOUBLE_EQ(constructed.area(), 6);
        }
        EXPECT_TRUE(PolygonAllocationState::OWNERS.empty());
    }

    TEST(PolygonTest, AllocatorFailureLeavesBothGeometriesValid) {
        using Allocated = aho::Polygon<double, vsl::D2,
                                       aho::coordinate::DefaultCoordinateInfo<vsl::D2>::Value, std::size_t, PolygonAllocator<I>>;
        const std::array points{P(0, 0), P(2, 0), P(0, 3)};
        const std::array indices{I{{0}, {1}, {2}}, I{{0}, {2}, {1}}};
        {
            Allocated source(points, indices), destination(points, std::span{indices}.first(1));
            PolygonAllocationState::FAIL_NEXT = true;
            EXPECT_THROW(destination = std::move(source), std::bad_alloc);
            EXPECT_EQ(source.size(), 2u);
            EXPECT_EQ(destination.size(), 1u);
            EXPECT_DOUBLE_EQ(source.area(), 6);
            EXPECT_DOUBLE_EQ(destination.area(), 3);
        }
        EXPECT_TRUE(PolygonAllocationState::OWNERS.empty());
    }
    TEST(PolygonTest, AllocatorConstructionFailureRestoresSourceGeometry) {
        using Allocated = aho::Polygon<double, vsl::D2,
                                       aho::coordinate::DefaultCoordinateInfo<vsl::D2>::Value, std::size_t, PolygonAllocator<I>>;
        const std::array points{P(0, 0), P(2, 0), P(0, 3)};
        const std::array indices{I{{0}, {1}, {2}}, I{{0}, {2}, {1}}};
        {
            Allocated source(points, indices), destination;
            const auto* data = source.points().data();
            PolygonAllocationState::CONSTRUCT_BEFORE_FAILURE = 1;
            EXPECT_THROW(destination = std::move(source), std::runtime_error);
            EXPECT_EQ(source.points().data(), data);
            EXPECT_EQ(source.size(), 2u);
            EXPECT_DOUBLE_EQ(source.area(), 6);
            EXPECT_TRUE(destination.points().empty());
            EXPECT_TRUE(destination.indices().empty());
        }
        EXPECT_TRUE(PolygonAllocationState::OWNERS.empty());
    }
} // namespace
