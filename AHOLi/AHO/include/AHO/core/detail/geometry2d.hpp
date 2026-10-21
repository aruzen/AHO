#pragma once
#include "../point.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <span>
#include <vector>

namespace AHO_NAMESPACE::detail::geometry2d {
    using Point = AHO_NAMESPACE::d2::Point<long double>;

    // ベクトルabとacの2D外積。三角形abcの符号付き面積の2倍を返す。
    inline long double cross(Point a, Point b, Point c);

    // 点列a→b→cの向きを外積の符号（1/-1）で返す。演算誤差の範囲内なら共線とみなして0。
    inline int orientation(Point a, Point b, Point c);

    // 点pが線分ab上にあるか。両端点を含み、共線性はorientationで判定する。
    inline bool onSegment(Point a, Point b, Point p);

    // 線分abとcdの交差判定。端点の接触と、共線な線分の重なりも含む。
    inline bool intersects(Point a, Point b, Point c, Point d);

    // 点pが三角形abcの内部・辺上・頂点にあるか。orientationで共線とみなす退化三角形はfalse。
    inline bool inTriangle(Point a, Point b, Point c, Point p);

    template<typename P>
    Point convert(const P& p);

    // 全座標を同じ正の係数で縮小し、差と外積のオーバーフローを避ける。
    inline void normalize(std::span<Point> points);

    inline long double cross(Point a, Point b, Point c) {
        return (b - a).cross(c - a).value.___AN1;
    }

    inline int orientation(Point a, Point b, Point c) {
        const auto u = (b.value.___AN1 - a.value.___AN1) * (c.value.___AN2 - a.value.___AN2);
        const auto v = (b.value.___AN2 - a.value.___AN2) * (c.value.___AN1 - a.value.___AN1);
        const auto error = 16 * std::numeric_limits<long double>::epsilon() * (std::abs(u) + std::abs(v));
        return u - v > error ? 1 : (u - v < -error ? -1 : 0);
    }

    inline bool onSegment(Point a, Point b, Point p) {
        return orientation(a, b, p) == 0 &&
               p.value.___AN1 >= std::min(a.value.___AN1, b.value.___AN1) && p.value.___AN1 <= std::max(a.value.___AN1, b.value.___AN1) &&
               p.value.___AN2 >= std::min(a.value.___AN2, b.value.___AN2) && p.value.___AN2 <= std::max(a.value.___AN2, b.value.___AN2);
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
            scale = std::max({scale, std::abs(p.value.___AN1), std::abs(p.value.___AN2)});
        if (scale > 0)
            for (auto& p : points)
                p /= scale;
    }
} // namespace AHO_NAMESPACE::detail::geometry2d
