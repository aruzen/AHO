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
        Polygon(std::span<const point_type> points, std::span<const triangle_type> triangles) {
            if (points.size() && points.size() - 1 > std::numeric_limits<Size>::max())
                throw std::length_error("Polygon vertex index exceeds index type");
            for (const auto& triangle : triangles)
                for (const auto index : {triangle.idx1.index, triangle.idx2.index, triangle.idx3.index})
                    if (index >= points.size())
                        throw std::out_of_range("Polygon triangle index exceeds vertex count");
            points_.assign(points.begin(), points.end());
            triangles_.assign(triangles.begin(), triangles.end());
        }

        [[nodiscard]] std::span<const point_type> points() const noexcept { return points_; }
        [[nodiscard]] std::span<const triangle_type> indices() const noexcept { return triangles_; }
        [[nodiscard]] std::size_t size() const noexcept { return triangles_.size(); }
        [[nodiscard]] bool empty() const noexcept { return triangles_.empty(); }

        [[nodiscard]] auto triangles() const {
            return std::views::all(triangles_) | std::views::transform([this](const triangle_type& t) {
                       return Triangle<R, D, CI>{points_[t.idx1.index], points_[t.idx2.index], points_[t.idx3.index]};
                   });
        }
        // 各三角形の3辺を返す。共有辺も含むため、輪郭線とは区別する。
        [[nodiscard]] auto lines() const {
            return std::views::iota(std::size_t{0}, triangles_.size() * 3) | std::views::transform([this](std::size_t i) {
                       const auto& t = triangles_[i / 3];
                       const Size vertices[]{t.idx1.index, t.idx2.index, t.idx3.index};
                       return Line<R, D, CI>{points_[vertices[i % 3]], points_[vertices[(i + 1) % 3]]};
                   });
        }
        [[nodiscard]] double area() const {
            long double result = 0;
            for (const auto triangle : triangles()) {
                const auto a = triangle.pos1.value;
                const auto b = triangle.pos2.value;
                const auto c = triangle.pos3.value;
                const long double ux = static_cast<long double>(b.___AN1) - static_cast<long double>(a.___AN1);
                const long double vx = static_cast<long double>(c.___AN1) - static_cast<long double>(a.___AN1);
                if constexpr (D::dimension == 2) {
                    const long double uy = static_cast<long double>(b.___AN2) - static_cast<long double>(a.___AN2);
                    const long double vy = static_cast<long double>(c.___AN2) - static_cast<long double>(a.___AN2);
                    result += std::abs(ux * vy - uy * vx) / 2;
                } else if constexpr (D::dimension == 3) {
                    const long double uy = static_cast<long double>(b.___AN2) - static_cast<long double>(a.___AN2);
                    const long double vy = static_cast<long double>(c.___AN2) - static_cast<long double>(a.___AN2);
                    const long double uz = static_cast<long double>(b.___AN3) - static_cast<long double>(a.___AN3);
                    const long double vz = static_cast<long double>(c.___AN3) - static_cast<long double>(a.___AN3);
                    result += std::hypot(uy * vz - uz * vy, uz * vx - ux * vz, ux * vy - uy * vx) / 2;
                }
            }
            return static_cast<double>(result);
        }

    private:
        std::vector<point_type> points_;
        std::vector<triangle_type, Alloc> triangles_;
    };
} // namespace AHO_NAMESPACE
