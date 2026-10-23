#pragma once
#include "../../core/polygon.hpp"
#include <saitekikuukan/soa.hpp>
#include <span>
#include <stdexcept>
#include <utility>

namespace AHO_NAMESPACE::x {
    template<typename PolygonType, typename VertexData, typename TriangleData,
             typename VertexStorage = SAITEKIKUUKAN_NAMESPACE::SoA<VertexData>,
             typename TriangleStorage = SAITEKIKUUKAN_NAMESPACE::SoA<TriangleData>>
    class Mesh {
        static_assert(std::same_as<typename VertexStorage::ValueType, VertexData>);
        static_assert(std::same_as<typename TriangleStorage::ValueType, TriangleData>);
        static_assert(std::is_nothrow_swappable_v<PolygonType>);
        static_assert(std::is_nothrow_swappable_v<VertexStorage>);
        static_assert(std::is_nothrow_swappable_v<TriangleStorage>);

        PolygonType _geometry;
        VertexStorage _vertex_data;
        TriangleStorage _triangle_data;

    public:
        using Dimension = typename PolygonType::Dimension;
        using ElementType = typename PolygonType::ElementType;
        using CoordinateInfo = typename PolygonType::CoordinateInfo;
        using PointType = typename PolygonType::PointType;
        using IndexType = typename PolygonType::IndexType;
        using TriangleType = typename PolygonType::TriangleType;
        using VertexDataType = VertexData;
        using TriangleDataType = TriangleData;

        Mesh() = default;
        explicit Mesh(PolygonType geometry);
        Mesh(PolygonType geometry, std::span<const VertexData> vertices, std::span<const TriangleData> triangles);
        Mesh(const Mesh&) = default;
        Mesh(Mesh&& other) noexcept(std::is_nothrow_default_constructible_v<PolygonType> &&
                                    std::is_nothrow_default_constructible_v<VertexStorage> &&
                                    std::is_nothrow_default_constructible_v<TriangleStorage>);
        Mesh& operator=(Mesh other) noexcept;
        void swap(Mesh& other) noexcept;

        [[nodiscard]] const PolygonType& geometry() const noexcept;
        [[nodiscard]] const VertexStorage& vertex_data() const noexcept;
        [[nodiscard]] const TriangleStorage& triangle_data() const noexcept;
        [[nodiscard]] std::size_t vertex_count() const noexcept;
        [[nodiscard]] std::size_t size() const noexcept;
        [[nodiscard]] bool empty() const noexcept;

        template<auto Member>
        [[nodiscard]] auto vertex_column() noexcept;
        template<auto Member>
        [[nodiscard]] auto vertex_column() const noexcept;
        template<auto Member>
        [[nodiscard]] auto triangle_column() noexcept;
        template<auto Member>
        [[nodiscard]] auto triangle_column() const noexcept;
        [[nodiscard]] auto vertex(std::size_t index);
        [[nodiscard]] auto vertex(std::size_t index) const;
        [[nodiscard]] auto triangle(std::size_t index);
        [[nodiscard]] auto triangle(std::size_t index) const;

        [[nodiscard]] auto points() const noexcept;
        [[nodiscard]] auto indices() const noexcept;
        [[nodiscard]] auto triangles() const;
        [[nodiscard]] auto lines() const;
        [[nodiscard]] double area() const;
    };

    template<typename P, typename V, typename T, typename VS, typename TS>
    Mesh<P, V, T, VS, TS>::Mesh(P geometry)
        : _geometry(std::move(geometry)), _vertex_data(_geometry.points().size()),
          _triangle_data(_geometry.indices().size()) {}

    template<typename P, typename V, typename T, typename VS, typename TS>
    Mesh<P, V, T, VS, TS>::Mesh(P geometry, std::span<const V> vertices, std::span<const T> triangles)
        : _geometry(std::move(geometry)) {
        if (vertices.size() != _geometry.points().size())
            throw std::invalid_argument("Mesh vertex attribute count differs from geometry");
        if (triangles.size() != _geometry.indices().size())
            throw std::invalid_argument("Mesh triangle attribute count differs from geometry");
        _vertex_data = VS(vertices);
        _triangle_data = TS(triangles);
    }

    template<typename P, typename V, typename T, typename VS, typename TS>
    Mesh<P, V, T, VS, TS>::Mesh(Mesh&& other) noexcept(std::is_nothrow_default_constructible_v<P> &&
                                                       std::is_nothrow_default_constructible_v<VS> &&
                                                       std::is_nothrow_default_constructible_v<TS>)
        : Mesh() {
        swap(other);
    }

    template<typename P, typename V, typename T, typename VS, typename TS>
    Mesh<P, V, T, VS, TS>& Mesh<P, V, T, VS, TS>::operator=(Mesh other) noexcept {
        swap(other);
        return *this;
    }

    template<typename P, typename V, typename T, typename VS, typename TS>
    void Mesh<P, V, T, VS, TS>::swap(Mesh& other) noexcept {
        using std::swap;
        swap(_geometry, other._geometry);
        swap(_vertex_data, other._vertex_data);
        swap(_triangle_data, other._triangle_data);
    }

    template<typename P, typename V, typename T, typename VS, typename TS>
    const P& Mesh<P, V, T, VS, TS>::geometry() const noexcept {
        return _geometry;
    }

    template<typename P, typename V, typename T, typename VS, typename TS>
    const VS& Mesh<P, V, T, VS, TS>::vertex_data() const noexcept {
        return _vertex_data;
    }

    template<typename P, typename V, typename T, typename VS, typename TS>
    const TS& Mesh<P, V, T, VS, TS>::triangle_data() const noexcept {
        return _triangle_data;
    }

    template<typename P, typename V, typename T, typename VS, typename TS>
    std::size_t Mesh<P, V, T, VS, TS>::vertex_count() const noexcept {
        return _geometry.points().size();
    }

    template<typename P, typename V, typename T, typename VS, typename TS>
    std::size_t Mesh<P, V, T, VS, TS>::size() const noexcept {
        return _geometry.indices().size();
    }

    template<typename P, typename V, typename T, typename VS, typename TS>
    bool Mesh<P, V, T, VS, TS>::empty() const noexcept {
        return _geometry.empty();
    }

    template<typename P, typename V, typename T, typename VS, typename TS>
    template<auto Member>
    auto Mesh<P, V, T, VS, TS>::vertex_column() noexcept {
        return _vertex_data.template column<Member>();
    }

    template<typename P, typename V, typename T, typename VS, typename TS>
    template<auto Member>
    auto Mesh<P, V, T, VS, TS>::vertex_column() const noexcept {
        return _vertex_data.template column<Member>();
    }

    template<typename P, typename V, typename T, typename VS, typename TS>
    template<auto Member>
    auto Mesh<P, V, T, VS, TS>::triangle_column() noexcept {
        return _triangle_data.template column<Member>();
    }

    template<typename P, typename V, typename T, typename VS, typename TS>
    template<auto Member>
    auto Mesh<P, V, T, VS, TS>::triangle_column() const noexcept {
        return _triangle_data.template column<Member>();
    }

    template<typename P, typename V, typename T, typename VS, typename TS>
    auto Mesh<P, V, T, VS, TS>::vertex(std::size_t index) {
        return _vertex_data.at(index);
    }

    template<typename P, typename V, typename T, typename VS, typename TS>
    auto Mesh<P, V, T, VS, TS>::vertex(std::size_t index) const {
        return _vertex_data.at(index);
    }

    template<typename P, typename V, typename T, typename VS, typename TS>
    auto Mesh<P, V, T, VS, TS>::triangle(std::size_t index) {
        return _triangle_data.at(index);
    }

    template<typename P, typename V, typename T, typename VS, typename TS>
    auto Mesh<P, V, T, VS, TS>::triangle(std::size_t index) const {
        return _triangle_data.at(index);
    }

    template<typename P, typename V, typename T, typename VS, typename TS>
    auto Mesh<P, V, T, VS, TS>::points() const noexcept {
        return _geometry.points();
    }

    template<typename P, typename V, typename T, typename VS, typename TS>
    auto Mesh<P, V, T, VS, TS>::indices() const noexcept {
        return _geometry.indices();
    }

    template<typename P, typename V, typename T, typename VS, typename TS>
    auto Mesh<P, V, T, VS, TS>::triangles() const {
        return _geometry.triangles();
    }

    template<typename P, typename V, typename T, typename VS, typename TS>
    auto Mesh<P, V, T, VS, TS>::lines() const {
        return _geometry.lines();
    }

    template<typename P, typename V, typename T, typename VS, typename TS>
    double Mesh<P, V, T, VS, TS>::area() const {
        return _geometry.area();
    }
} // namespace AHO_NAMESPACE::x
