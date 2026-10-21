#pragma once
#include "triangle.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <ranges>
#include <span>
#include <stdexcept>
#include <type_traits>
#include <vector>

namespace AHO_NAMESPACE {
    // 頂点と三角形を所有するメッシュ。参照ビューから形状を変更できない。
    template<typename R, VSL_NAMESPACE::is_dimension D, typename CI, typename Size, typename Alloc>
    struct Polygon {
        using dimension = D;
        using element_type = R;
        using coordinate_info = CI;
        using point_type = _Point<R, D, CI>;
        using index_type = Size;
        using triangle_type = IndexedTriangle<Size>;
        static_assert(std::is_integral_v<Size> && std::is_unsigned_v<Size> && !std::same_as<Size, bool>);

        Polygon() = default;
        Polygon(std::span<const point_type> points, std::span<const triangle_type> triangles);

        [[nodiscard]] std::span<const point_type> points() const noexcept;
        [[nodiscard]] std::span<const triangle_type> indices() const noexcept;
        [[nodiscard]] std::size_t size() const noexcept;
        [[nodiscard]] bool empty() const noexcept;

        [[nodiscard]] auto triangles() const;
        // 各三角形の3辺を返す。共有辺も含むため、輪郭線とは区別する。
        [[nodiscard]] auto lines() const;
        [[nodiscard]] double area() const;

    private:
        std::vector<point_type> _points;
        std::vector<triangle_type, Alloc> _triangles;
    };

    template<typename R, VSL_NAMESPACE::is_dimension D, typename CI, typename Size, typename Alloc>
    Polygon<R, D, CI, Size, Alloc>::Polygon(std::span<const point_type> points,
                                            std::span<const triangle_type> triangles) {
        if (points.size() && points.size() - 1 > std::numeric_limits<Size>::max())
            throw std::length_error("Polygon vertex index exceeds index type");
        for (const auto& triangle : triangles)
            for (const auto index : {triangle.idx1.index, triangle.idx2.index, triangle.idx3.index})
                if (index >= points.size())
                    throw std::out_of_range("Polygon triangle index exceeds vertex count");
        _points.assign(points.begin(), points.end());
        _triangles.assign(triangles.begin(), triangles.end());
    }

    template<typename R, VSL_NAMESPACE::is_dimension D, typename CI, typename Size, typename Alloc>
    std::span<const typename Polygon<R, D, CI, Size, Alloc>::point_type> Polygon<R, D, CI, Size, Alloc>::points() const noexcept {
        return _points;
    }

    template<typename R, VSL_NAMESPACE::is_dimension D, typename CI, typename Size, typename Alloc>
    std::span<const typename Polygon<R, D, CI, Size, Alloc>::triangle_type> Polygon<R, D, CI, Size, Alloc>::indices() const noexcept {
        return _triangles;
    }

    template<typename R, VSL_NAMESPACE::is_dimension D, typename CI, typename Size, typename Alloc>
    std::size_t Polygon<R, D, CI, Size, Alloc>::size() const noexcept {
        return _triangles.size();
    }

    template<typename R, VSL_NAMESPACE::is_dimension D, typename CI, typename Size, typename Alloc>
    bool Polygon<R, D, CI, Size, Alloc>::empty() const noexcept {
        return _triangles.empty();
    }

    template<typename R, VSL_NAMESPACE::is_dimension D, typename CI, typename Size, typename Alloc>
    auto Polygon<R, D, CI, Size, Alloc>::triangles() const {
        return std::views::all(_triangles) | std::views::transform([this](const triangle_type& t) {
                   return Triangle<R, D, CI>{_points[t.idx1.index], _points[t.idx2.index], _points[t.idx3.index]};
               });
    }

    template<typename R, VSL_NAMESPACE::is_dimension D, typename CI, typename Size, typename Alloc>
    auto Polygon<R, D, CI, Size, Alloc>::lines() const {
        return std::views::iota(std::size_t{0}, _triangles.size() * 3) | std::views::transform([this](std::size_t i) {
                   const auto& t = _triangles[i / 3];
                   const Size vertices[]{t.idx1.index, t.idx2.index, t.idx3.index};
                   return Line<R, D, CI>{_points[vertices[i % 3]], _points[vertices[(i + 1) % 3]]};
               });
    }

    template<typename R, VSL_NAMESPACE::is_dimension D, typename CI, typename Size, typename Alloc>
    double Polygon<R, D, CI, Size, Alloc>::area() const {
        long double result = 0;
        for (const auto triangle : triangles()) {
            if constexpr (D::dimension == 2 || D::dimension == 3) {
                const auto origin = triangle.pos1.template cast<long double>();
                const auto ab = triangle.pos2.template cast<long double>() - origin;
                const auto ac = triangle.pos3.template cast<long double>() - origin;
                result += ab.cross(ac).length() / 2;
            }
        }
        return static_cast<double>(result);
    }
} // namespace AHO_NAMESPACE
