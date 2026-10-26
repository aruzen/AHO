#include <AHO/x/core/mesh.hpp>
#include <gtest/gtest.h>
#include <array>
#include <cstdint>
#include <string>
#ifdef AHO_MESH_GPU_TESTS
#include "support/vulkan_fixture.hpp"
#endif

namespace {
    struct VertexData {
        aho::d3::VectorF normal{0, 0, 1};
        std::array<float, 2> uv0{}, uv1{};
    };
    struct TriangleData {
        std::uint32_t material = 7;
    };
    struct NoData {};
    struct Label {
        std::string text;
    };
} // namespace
#if !defined(__cpp_impl_reflection) || __cpp_impl_reflection < 202506L
template<>
struct aho::x::MeshAttributeSchema<VertexData> {
    using Members = aho::x::MeshAttributeMembers<&VertexData::normal, &VertexData::uv0, &VertexData::uv1>;
};
template<>
struct aho::x::MeshAttributeSchema<TriangleData> {
    using Members = aho::x::MeshAttributeMembers<&TriangleData::material>;
};
template<>
struct aho::x::MeshAttributeSchema<NoData> {
    using Members = aho::x::MeshAttributeMembers<>;
};
template<>
struct aho::x::MeshAttributeSchema<Label> {
    using Members = aho::x::MeshAttributeMembers<&Label::text>;
};
#endif

namespace {
    using Polygon = aho::Polygon<float, vsl::D3>;
    using Mesh = aho::x::Mesh<Polygon, VertexData, TriangleData>;
    const std::array POINTS{aho::d3::PointF(0, 0, 0), aho::d3::PointF(2, 0, 0), aho::d3::PointF(0, 3, 0)};
    const std::array INDICES{aho::IndexedTriangle<std::size_t>{{0}, {1}, {2}}};

    static_assert(aho::concepts::is_points_object<Mesh>);
    static_assert(aho::concepts::is_lines_object<Mesh>);
    static_assert(aho::concepts::is_triangles_object<Mesh>);
    static_assert(std::same_as<Mesh::TriangleType, Polygon::TriangleType>);
    static_assert(std::same_as<Mesh::TriangleDataType, TriangleData>);
    static_assert(std::same_as<decltype(std::declval<Mesh&>().geometry()), const Polygon&>);
    static_assert(std::same_as<decltype(std::declval<Mesh&>().vertex_data()), const aho::x::MeshAttributes<VertexData>&>);
    static_assert(std::same_as<decltype(std::declval<const Mesh&>().triangle_column<&TriangleData::material>()), std::span<const std::uint32_t>>);

    TEST(ExperimentalMeshTest, DefaultIsEmptyAndRejectsAttributeAccess) {
        Mesh mesh;
        EXPECT_TRUE(mesh.empty());
        EXPECT_EQ(mesh.vertex_count(), 0u);
        EXPECT_EQ(mesh.size(), 0u);
        EXPECT_TRUE(mesh.vertex_data().empty());
        EXPECT_TRUE(mesh.triangle_data().empty());
        EXPECT_THROW((void)mesh.vertex(0), std::out_of_range);
        EXPECT_THROW((void)std::as_const(mesh).triangle(0), std::out_of_range);
    }

    TEST(ExperimentalMeshTest, GeometryAndAttributeCountsStayTogether) {
        Mesh mesh{Polygon(POINTS, INDICES)};
        EXPECT_EQ(mesh.vertex_count(), 3u);
        EXPECT_EQ(mesh.vertex_data().size(), 3u);
        EXPECT_EQ(mesh.size(), 1u);
        EXPECT_EQ(mesh.triangle_data().size(), 1u);
        EXPECT_EQ(mesh.triangle(0).read().material, 7u);
        EXPECT_EQ(mesh.vertex(0).read().normal.value.___AN3, 1);
        EXPECT_DOUBLE_EQ(mesh.area(), 3);
        EXPECT_EQ(std::ranges::distance(mesh.triangles()), 1);
        EXPECT_EQ(std::ranges::distance(mesh.lines()), 3);
        EXPECT_EQ(mesh.points().size(), 3u);
        EXPECT_EQ(mesh.indices()[0].idx3.index, 2u);
        EXPECT_EQ(&mesh.geometry().points()[0], &mesh.points()[0]);
        EXPECT_THROW((void)mesh.vertex(3), std::out_of_range);
        EXPECT_THROW((void)mesh.triangle(1), std::out_of_range);
    }

    TEST(ExperimentalMeshTest, ExplicitRowsBecomeIndependentColumns) {
        std::array<VertexData, 3> vertices{};
        vertices[0].uv0 = {1, 2};
        vertices[0].uv1 = {3, 4};
        std::array triangles{TriangleData{42}};
        Mesh mesh{Polygon(POINTS, INDICES), vertices, triangles};
        vertices[0].uv0[0] = 99;
        triangles[0].material = 99;
        EXPECT_EQ(mesh.vertex_column<&VertexData::uv0>()[0][0], 1);
        mesh.vertex_column<&VertexData::uv0>()[0][0] = 10;
        EXPECT_EQ(mesh.vertex_column<&VertexData::uv1>()[0][0], 3);
        mesh.triangle_column<&TriangleData::material>()[0] = 24;
        EXPECT_EQ(std::as_const(mesh).triangle(0).read().material, 24u);
        EXPECT_EQ(std::as_const(mesh).vertex(0).read().uv0[0], 10);
        EXPECT_EQ(&mesh.vertex_column<&VertexData::uv0>()[1], &mesh.vertex_column<&VertexData::uv0>()[0] + 1);
        mesh.vertex(1).write(VertexData{{0, 1, 0}, {5, 6}, {7, 8}});
        mesh.triangle(0).write(TriangleData{88});
        EXPECT_EQ(mesh.vertex(1).read().uv1[1], 8);
        EXPECT_EQ(mesh.triangle(0).read().material, 88u);
    }

    TEST(ExperimentalMeshTest, RejectsVertexAndTriangleAttributeCountMismatch) {
        const std::array<VertexData, 3> vertices{};
        const std::array<TriangleData, 1> triangles{};
        EXPECT_THROW((Mesh{Polygon(POINTS, INDICES), std::span{vertices}.first(2), triangles}), std::invalid_argument);
        EXPECT_THROW((Mesh{Polygon(POINTS, INDICES), vertices, std::span<const TriangleData>{}}), std::invalid_argument);
    }

    TEST(ExperimentalMeshTest, CopyMoveAssignmentAndSwapPreserveCompleteMeshes) {
        Mesh source{Polygon(POINTS, INDICES)};
        source.triangle(0).write(TriangleData{123});
        auto copy = source;
        copy.triangle(0).write(TriangleData{456});
        EXPECT_EQ(source.triangle(0).read().material, 123u);
        Mesh moved{std::move(copy)};
        EXPECT_EQ(moved.triangle(0).read().material, 456u);
        EXPECT_TRUE(copy.empty());
        EXPECT_TRUE(copy.vertex_data().empty());
        EXPECT_TRUE(copy.triangle_data().empty());
        Mesh assigned;
        assigned = source;
        EXPECT_EQ(assigned.vertex_count(), 3u);
        EXPECT_EQ(assigned.triangle(0).read().material, 123u);
        assigned = std::move(moved);
        EXPECT_EQ(assigned.triangle(0).read().material, 456u);
        EXPECT_TRUE(moved.empty());
        source.swap(moved);
        EXPECT_TRUE(source.empty());
        EXPECT_EQ(moved.vertex_count(), moved.vertex_data().size());
        EXPECT_EQ(moved.size(), moved.triangle_data().size());
    }

    TEST(ExperimentalMeshTest, EmptyAttributesAndNonTrivialFieldsAreSupported) {
        aho::x::Mesh<Polygon, NoData, Label> mesh{Polygon(POINTS, INDICES)};
        EXPECT_EQ(mesh.vertex_data().size(), 3u);
        mesh.triangle(0).write(Label{"triangle"});
        EXPECT_EQ(mesh.triangle(0).read().text, "triangle");
        EXPECT_NO_THROW(mesh.vertex(2).write(NoData{}));
    }

    TEST(ExperimentalMeshTest, CustomStorageCanChangeColumnOrderWithoutChangingFieldAccess) {
        using Vertices = aho::x::MeshAttributes<VertexData, aho::x::MeshAttributeMembers<&VertexData::uv1, &VertexData::uv0, &VertexData::normal>>;
        aho::x::Mesh<Polygon, VertexData, TriangleData, Vertices> mesh{Polygon(POINTS, INDICES)};
        mesh.vertex_column<&VertexData::uv1>()[0] = {7, 8};
        EXPECT_EQ(mesh.vertex(0).read().uv1[0], 7);
        EXPECT_EQ(mesh.vertex(0).read().uv0[0], 0);
        EXPECT_EQ(mesh.vertex_data().size(), mesh.vertex_count());
    }

    TEST(ExperimentalMeshTest, BoundedAttributesKeepGeometryAndColumnsTogetherAcrossCopyAndMove) {
        using Vertices = aho::x::MeshAttributes<VertexData, void, kk::storage::fixed_layout<3>>;
        using Triangles = aho::x::MeshAttributes<TriangleData, void, kk::storage::dynamic_layout<1>>;
        using BoundedMesh = aho::x::Mesh<Polygon, VertexData, TriangleData, Vertices, Triangles>;
        BoundedMesh mesh{Polygon(POINTS, INDICES)};
        EXPECT_EQ(mesh.vertex_count(), 3u);
        EXPECT_EQ(mesh.vertex_column<&VertexData::uv0>().size(), 3u);
        EXPECT_EQ(mesh.triangle(0).read().material, 7u);
        auto copy = mesh;
        copy.vertex_column<&VertexData::uv0>()[0] = {7, 8};
        auto moved = std::move(copy);
        EXPECT_TRUE(copy.empty());
        EXPECT_TRUE(copy.vertex_data().empty());
        EXPECT_EQ(moved.vertex(0).read().uv0[1], 8);
        EXPECT_EQ(mesh.vertex(0).read().uv0[0], 0);
        const std::array points{POINTS[0], POINTS[1], POINTS[2], POINTS[0]};
        EXPECT_THROW((void)BoundedMesh{Polygon(points, INDICES)}, std::length_error);
        EXPECT_EQ(mesh.vertex_count(), 3u);
    }

    TEST(ExperimentalMeshTest, DuplicatePositionsRetainDistinctUVsAndNarrowIndices) {
        using Geometry = aho::Polygon<float, vsl::D2,
                                      aho::coordinate::DefaultCoordinateInfo<vsl::D2>::Value, std::uint16_t>;
        const std::array points{aho::d2::PointF(0, 0), aho::d2::PointF(1, 0), aho::d2::PointF(0, 1), aho::d2::PointF(0, 0)};
        const std::array indices{aho::IndexedTriangle<std::uint16_t>{{3}, {1}, {2}}};
        aho::x::Mesh<Geometry, VertexData, TriangleData> mesh{Geometry(points, indices)};
        mesh.vertex_column<&VertexData::uv0>()[0] = {0, 0};
        mesh.vertex_column<&VertexData::uv0>()[3] = {1, 1};
        EXPECT_EQ(mesh.vertex_count(), 4u);
        EXPECT_EQ(mesh.indices()[0].idx1.index, 3u);
        EXPECT_EQ(mesh.vertex(0).read().uv0[0], 0);
        EXPECT_EQ(mesh.vertex(3).read().uv0[0], 1);
        EXPECT_DOUBLE_EQ(mesh.area(), 0.5);
    }
} // namespace

#ifdef AHO_MESH_GPU_TESTS
namespace {
    class MeshVulkanIntegrationTest : public vsl_test::VulkanIntegrationTest {
        bool validate_vulkan() const override;
    };

    bool MeshVulkanIntegrationTest::validate_vulkan() const {
        return true;
    }

    TEST_F(MeshVulkanIntegrationTest, ColumnUploadsAndReadbackReuseExistingBuffers) {
        if (!host_coherent_available())
            GTEST_SKIP() << "HostVisible|HostCoherent memory unavailable";
        Mesh mesh{Polygon(POINTS, INDICES)};
        mesh.vertex_column<&VertexData::uv0>()[0] = {0, 0};
        mesh.vertex_column<&VertexData::uv0>()[1] = {1, 0};
        mesh.vertex_column<&VertexData::uv0>()[2] = {0, 1};
        const auto uvs = std::as_const(mesh).vertex_column<&VertexData::uv0>();
        auto manager = make_manager();
        vsl::StagingBuffer<vsl::MemoryType::TRANSFER_SOURCE> source(device, uvs.data(), uvs.size());
        vsl::DeviceLocalBuffer<vsl::MemoryType::VERTEX_BUFFER | vsl::MemoryType::TRANSFER_SOURCE> gpu(device, uvs.size_bytes());
        vsl::StagingBuffer<vsl::MemoryType::TRANSFER_DESTINATION> destination(device, uvs.size_bytes());
        gpu.copy_by_buffer(manager, &source);
        destination.copy_by_buffer(manager, &gpu);
        const std::array<float, 6> expected{0, 0, 1, 0, 0, 1};
        {
            const auto mapped = destination.data(uvs.size_bytes());
            EXPECT_EQ(std::memcmp(mapped.data, expected.data(), sizeof(expected)), 0);
        }
        mesh.vertex_column<&VertexData::uv0>()[1] = {0.25f, 0.75f};
        ASSERT_TRUE(source.copy(uvs.data(), uvs.size()));
        gpu.copy_by_buffer(manager, &source);
        destination.copy_by_buffer(manager, &gpu);
        const std::array<float, 6> changed{0, 0, 0.25f, 0.75f, 0, 1};
        const auto mapped = destination.data(uvs.size_bytes());
        EXPECT_EQ(std::memcmp(mapped.data, changed.data(), sizeof(changed)), 0);
    }
} // namespace
#endif
