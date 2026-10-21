#pragma once
#include "../../define.hpp"
#include "../math/coordinate.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <span>
#include <vector>

namespace AHO_NAMESPACE::detail::geometry2d {
    struct Point {
        long double x, y;
    };

    inline long double cross(Point a, Point b, Point c);
    inline int orientation(Point a, Point b, Point c);
    inline bool onSegment(Point a, Point b, Point p);
    inline bool intersects(Point a, Point b, Point c, Point d);
    inline bool inTriangle(Point a, Point b, Point c, Point p);

    template<typename P>
    Point convert(const P& p);

    // 全座標を同じ正の係数で縮小し、差と外積のオーバーフローを避ける。
    inline void normalize(std::span<Point> points);

    inline long double cross(Point a, Point b, Point c) {
        return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
    }

    inline int orientation(Point a, Point b, Point c) {
        const auto u = (b.x - a.x) * (c.y - a.y);
        const auto v = (b.y - a.y) * (c.x - a.x);
        const auto error = 16 * std::numeric_limits<long double>::epsilon() * (std::abs(u) + std::abs(v));
        return u - v > error ? 1 : (u - v < -error ? -1 : 0);
    }

    inline bool onSegment(Point a, Point b, Point p) {
        return orientation(a, b, p) == 0 && p.x >= std::min(a.x, b.x) && p.x <= std::max(a.x, b.x) &&
               p.y >= std::min(a.y, b.y) && p.y <= std::max(a.y, b.y);
    }

    inline bool intersects(Point a, Point b, Point c, Point d) {
        const int abC = orientation(a, b, c), abD = orientation(a, b, d);
        const int cdA = orientation(c, d, a), cdB = orientation(c, d, b);
        return (abC * abD < 0 && cdA * cdB < 0) || onSegment(a, b, c) || onSegment(a, b, d) ||
               onSegment(c, d, a) || onSegment(c, d, b);
    }

    inline bool inTriangle(Point a, Point b, Point c, Point p) {
        const int winding = orientation(a, b, c);
        return winding != 0 && orientation(a, b, p) * winding >= 0 && orientation(b, c, p) * winding >= 0 &&
               orientation(c, a, p) * winding >= 0;
    }

    template<typename P>
    Point convert(const P& p) {
        return {static_cast<long double>(p.value.___AN1), static_cast<long double>(p.value.___AN2)};
    }

    inline void normalize(std::span<Point> points) {
        long double scale = 0;
        for (const auto p : points)
            scale = std::max({scale, std::abs(p.x), std::abs(p.y)});
        if (scale > 0)
            for (auto& p : points) {
                p.x /= scale;
                p.y /= scale;
            }
    }
} // namespace AHO_NAMESPACE::detail::geometry2d
