#include <AHO/x/core/mesh.hpp>
#include <array>
#include <cstdint>
#include <iostream>

namespace {
    struct VertexData {
        aho::d3::VectorF normal{0, 0, 1};
        std::array<float, 2> uv0{}, uv1{};
    };

    struct TriangleData {
        std::uint32_t material = 0;
    };
} // namespace

template<>
struct kk::Schema<VertexData> {
    using Members = kk::MemberList<&VertexData::normal, &VertexData::uv0, &VertexData::uv1>;
};

template<>
struct kk::Schema<TriangleData> {
    using Members = kk::MemberList<&TriangleData::material>;
};

int main() {
    using Geometry = aho::Polygon<float, vsl::D3>;
    const std::array points{aho::d3::PointF(0, 0, 0), aho::d3::PointF(2, 0, 0), aho::d3::PointF(0, 3, 0)};
    const std::array indices{aho::IndexedTriangle<std::size_t>{{0}, {1}, {2}}};
    aho::x::Mesh<Geometry, VertexData, TriangleData> mesh{Geometry(points, indices)};

    auto uvs = mesh.vertex_column<&VertexData::uv0>();
    uvs[0] = {0, 0};
    uvs[1] = {1, 0};
    uvs[2] = {0, 1};
    mesh.triangle_column<&TriangleData::material>()[0] = 2;

    // uvs.data()/uvs.size_bytes()を、対応するGPU形式の転送元に使用できる。
    std::cout << "vertices=" << mesh.vertex_count()
              << " triangles=" << mesh.size()
              << " area=" << mesh.area()
              << " material=" << mesh.triangle(0).read().material << '\n';
}
