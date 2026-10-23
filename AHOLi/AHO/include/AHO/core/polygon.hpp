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
#include <memory>
#include <utility>
#include <saitekikuukan.hpp>

namespace AHO_NAMESPACE {
    namespace detail {
        template<typename Alloc>
        struct PolygonTriangleLayout : SAITEKIKUUKAN_NAMESPACE::storage::dynamic_layout<> {
            // saitekikuukanのLayout契約で指定された名前を使う。
            template<typename T>
            using column = std::vector<T, typename std::allocator_traits<Alloc>::template rebind_alloc<T>>; // NOLINT(readability-identifier-naming)
        };
    } // namespace detail

    // 頂点と三角形を所有するメッシュ。参照ビューから形状を変更できない。
    template<typename R, VSL_NAMESPACE::is_dimension D, typename CI, typename Size, typename Alloc>
    struct Polygon {
        using Dimension = D;
        using ElementType = R;
        using CoordinateInfo = CI;
        using PointType = Point_<R, D, CI>;
        using IndexType = Size;
        using TriangleType = IndexedTriangle<Size>;
        static_assert(std::is_integral_v<Size> && std::is_unsigned_v<Size> && !std::same_as<Size, bool>);

    private:
        using PointStorage = SAITEKIKUUKAN_NAMESPACE::soa<PointType>;
        using TriangleStorage = SAITEKIKUUKAN_NAMESPACE::basic_soa<detail::PolygonTriangleLayout<Alloc>, TriangleType>;
        static constexpr bool FAST_TRIANGLE_MOVE =
            std::allocator_traits<Alloc>::is_always_equal::value ||
            std::allocator_traits<Alloc>::propagate_on_container_swap::value;

        PointStorage _points;
        TriangleStorage _triangles;

    public:
        Polygon() = default;
        Polygon(const Polygon&) = default;
        Polygon(Polygon&&) noexcept(std::is_nothrow_move_constructible_v<PointStorage> &&
                                    std::is_nothrow_move_constructible_v<TriangleStorage>) = default;
        Polygon& operator=(const Polygon& other);
        Polygon& operator=(Polygon&& other) noexcept(FAST_TRIANGLE_MOVE);
        Polygon(std::span<const PointType> points, std::span<const TriangleType> triangles);

        [[nodiscard]] std::span<const PointType> points() const noexcept;
        [[nodiscard]] std::span<const TriangleType> indices() const noexcept;
        [[nodiscard]] std::size_t size() const noexcept;
        [[nodiscard]] bool empty() const noexcept;

        [[nodiscard]] auto triangles() const;
        // 各三角形の3辺を返す。共有辺も含むため、輪郭線とは区別する。
        [[nodiscard]] auto lines() const;
        [[nodiscard]] double area() const;
    };

    template<typename R, VSL_NAMESPACE::is_dimension D, typename CI, typename Size, typename Alloc>
    Polygon<R, D, CI, Size, Alloc>::Polygon(std::span<const PointType> points,
                                            std::span<const TriangleType> triangles) {
        if (points.size() && points.size() - 1 > std::numeric_limits<Size>::max())
            throw std::length_error("Polygon vertex index exceeds index type");
        for (const auto& triangle : triangles)
            for (const auto index : {triangle.idx1.index, triangle.idx2.index, triangle.idx3.index})
                if (index >= points.size())
                    throw std::out_of_range("Polygon triangle index exceeds vertex count");
        _points.reserve(points.size());
        _triangles.reserve(triangles.size());
        for (const auto& point : points)
            _points.push_back(point);
        for (const auto& triangle : triangles)
            _triangles.push_back(triangle);
    }

    template<typename R, VSL_NAMESPACE::is_dimension D, typename CI, typename Size, typename Alloc>
    Polygon<R, D, CI, Size, Alloc>& Polygon<R, D, CI, Size, Alloc>::operator=(const Polygon& other) {
        if (this != &other) {
            Polygon copy(other);
            *this = std::move(copy);
        }
        return *this;
    }

    template<typename R, VSL_NAMESPACE::is_dimension D, typename CI, typename Size, typename Alloc>
    Polygon<R, D, CI, Size, Alloc>& Polygon<R, D, CI, Size, Alloc>::operator=(Polygon&& other) noexcept(FAST_TRIANGLE_MOVE) {
        if (this == &other)
            return *this;
        if constexpr (FAST_TRIANGLE_MOVE) {
            _points = std::move(other._points);
            _triangles = std::move(other._triangles);
        } else {
            // 所有allocatorが異なる可能性があるため、列のswapを経由しない。
            _triangles.reserve(other.size());
            _triangles.clear();
            _points = std::move(other._points);
            try {
                for (const auto& triangle : other.indices())
                    _triangles.push_back(triangle);
            } catch (...) {
                other._points = std::move(_points);
                _triangles.clear();
                throw;
            }
            other._triangles.clear();
        }
        return *this;
    }

    template<typename R, VSL_NAMESPACE::is_dimension D, typename CI, typename Size, typename Alloc>
    std::span<const typename Polygon<R, D, CI, Size, Alloc>::PointType> Polygon<R, D, CI, Size, Alloc>::points() const noexcept {
        return _points.template column<0>();
    }

    template<typename R, VSL_NAMESPACE::is_dimension D, typename CI, typename Size, typename Alloc>
    std::span<const typename Polygon<R, D, CI, Size, Alloc>::TriangleType> Polygon<R, D, CI, Size, Alloc>::indices() const noexcept {
        return _triangles.template column<0>();
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
        return std::views::all(indices()) | std::views::transform([this](const TriangleType& t) {
                   const auto vertices = points();
                   return Triangle<R, D, CI>{vertices[t.idx1.index], vertices[t.idx2.index], vertices[t.idx3.index]};
               });
    }

    template<typename R, VSL_NAMESPACE::is_dimension D, typename CI, typename Size, typename Alloc>
    auto Polygon<R, D, CI, Size, Alloc>::lines() const {
        return std::views::iota(std::size_t{0}, _triangles.size() * 3) | std::views::transform([this](std::size_t i) {
                   const auto& t = indices()[i / 3];
                   const Size vertices[]{t.idx1.index, t.idx2.index, t.idx3.index};
                   const auto positions = points();
                   return Line<R, D, CI>{positions[vertices[i % 3]], positions[vertices[(i + 1) % 3]]};
               });
    }

    template<typename R, VSL_NAMESPACE::is_dimension D, typename CI, typename Size, typename Alloc>
    double Polygon<R, D, CI, Size, Alloc>::area() const {
        long double result = 0;
        for (const auto triangle : triangles()) {
            if constexpr (D::DIMENSION == 2 || D::DIMENSION == 3) {
                const auto origin = triangle.pos1.template cast<long double>();
                const auto ab = triangle.pos2.template cast<long double>() - origin;
                const auto ac = triangle.pos3.template cast<long double>() - origin;
                result += ab.cross(ac).length() / 2;
            }
        }
        return static_cast<double>(result);
    }
} // namespace AHO_NAMESPACE
