#pragma once
#include "polygon.hpp"
#include "detail/geometry2d.hpp"
#include <array>
#include <stdexcept>

namespace AHO_NAMESPACE::collision {
    namespace detail {
        template<typename T>
        concept Shape2D = AHO_NAMESPACE::concepts::is_triangles_object<T> &&
                          std::same_as<typename T::dimension, VSL_NAMESPACE::D2> &&
                          std::is_arithmetic_v<typename T::element_type> && requires(const T& shape) { shape.triangles(); };
        namespace g = AHO_NAMESPACE::detail::geometry2d;

        template<Shape2D Shape>
        void append(const Shape& shape, std::vector<g::Point>& points);
        inline bool contains(std::span<const g::Point, 3> t, g::Point p);
        inline bool intersects(std::span<const g::Point, 3> a, std::span<const g::Point, 3> b);
    } // namespace detail

    // 境界を含む閉集合として判定する。退化した三角形も線分・点として扱う。
    template<detail::Shape2D Shape, AHO_NAMESPACE::concepts::is_point P>
    requires std::same_as<typename P::dimension, VSL_NAMESPACE::D2> &&
             std::same_as<typename Shape::coordinate_info, typename P::coordinate_info> &&
             std::is_arithmetic_v<typename P::element_type>
    [[nodiscard]] bool contains(const Shape& shape, const P& point);

    template<detail::Shape2D Left, detail::Shape2D Right>
    requires std::same_as<typename Left::coordinate_info, typename Right::coordinate_info>
    [[nodiscard]] bool intersects(const Left& left, const Right& right);

    struct CollisionJudger {
        template<detail::Shape2D Left, detail::Shape2D Right>
        requires std::same_as<typename Left::coordinate_info, typename Right::coordinate_info>
        [[nodiscard]] bool operator()(const Left& left, const Right& right) const;
    };

    namespace detail {
        template<Shape2D Shape>
        void append(const Shape& shape, std::vector<g::Point>& points) {
            for (const Triangle<typename Shape::element_type, VSL_NAMESPACE::D2, typename Shape::coordinate_info> triangle : shape.triangles())
                for (const auto p : triangle.points()) {
                    const auto value = g::convert(p);
                    if (!std::isfinite(value.value.___AN1) || !std::isfinite(value.value.___AN2))
                        throw std::invalid_argument("Collision requires finite coordinates");
                    points.push_back(value);
                }
        }

        inline bool contains(std::span<const g::Point, 3> t, g::Point p) {
            return g::in_triangle(t[0], t[1], t[2], p) || g::on_segment(t[0], t[1], p) ||
                   g::on_segment(t[1], t[2], p) || g::on_segment(t[2], t[0], p);
        }

        inline bool intersects(std::span<const g::Point, 3> a, std::span<const g::Point, 3> b) {
            auto separated = [&](auto component) {
                const auto [a_min, a_max] = std::minmax({component(a[0]), component(a[1]), component(a[2])});
                const auto [b_min, b_max] = std::minmax({component(b[0]), component(b[1]), component(b[2])});
                return a_max < b_min || b_max < a_min;
            };
            if (separated([](g::Point p) { return p.value.___AN1; }) || separated([](g::Point p) { return p.value.___AN2; }))
                return false;
            for (std::size_t i = 0; i < 3; ++i)
                for (std::size_t j = 0; j < 3; ++j)
                    if (g::intersects(a[i], a[(i + 1) % 3], b[j], b[(j + 1) % 3]))
                        return true;
            return contains(a, b[0]) || contains(b, a[0]);
        }
    } // namespace detail

    template<detail::Shape2D Shape, AHO_NAMESPACE::concepts::is_point P>
    requires std::same_as<typename P::dimension, VSL_NAMESPACE::D2> &&
             std::same_as<typename Shape::coordinate_info, typename P::coordinate_info> &&
             std::is_arithmetic_v<typename P::element_type>
    [[nodiscard]] bool contains(const Shape& shape, const P& point) {
        std::vector<detail::g::Point> points;
        detail::append(shape, points);
        const auto p = detail::g::convert(point);
        if (!std::isfinite(p.value.___AN1) || !std::isfinite(p.value.___AN2))
            throw std::invalid_argument("Collision requires a finite point");
        points.push_back(p);
        detail::g::normalize(points);
        for (std::size_t i = 0; i + 3 < points.size(); i += 3)
            if (detail::contains(std::span<const detail::g::Point, 3>(points.data() + i, 3), points.back()))
                return true;
        return false;
    }

    template<detail::Shape2D Left, detail::Shape2D Right>
    requires std::same_as<typename Left::coordinate_info, typename Right::coordinate_info>
    [[nodiscard]] bool intersects(const Left& left, const Right& right) {
        std::vector<detail::g::Point> points;
        detail::append(left, points);
        const auto split = points.size();
        detail::append(right, points);
        detail::g::normalize(points);
        for (std::size_t i = 0; i < split; i += 3)
            for (std::size_t j = split; j < points.size(); j += 3)
                if (detail::intersects(std::span<const detail::g::Point, 3>(points.data() + i, 3),
                                       std::span<const detail::g::Point, 3>(points.data() + j, 3)))
                    return true;
        return false;
    }

    template<detail::Shape2D Left, detail::Shape2D Right>
    requires std::same_as<typename Left::coordinate_info, typename Right::coordinate_info>
    bool CollisionJudger::operator()(const Left& left, const Right& right) const {
        return intersects(left, right);
    }
} // namespace AHO_NAMESPACE::collision
