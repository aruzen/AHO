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
    inline bool on_segment(Point a, Point b, Point p);

    // 線分abとcdの交差判定。端点の接触と、共線な線分の重なりも含む。
    inline bool intersects(Point a, Point b, Point c, Point d);

    // 点pが三角形abcの内部・辺上・頂点にあるか。orientationで共線とみなす退化三角形はfalse。
    inline bool in_triangle(Point a, Point b, Point c, Point p);

    template<typename P>
    Point convert(const P& p);

    // 軸ごとに基数の冪で拡縮し、差と外積の桁あふれと不要な丸めを抑える。
    inline void normalize(std::span<Point> points);

    inline long double cross(Point a, Point b, Point c) {
        return (b - a).cross(c - a).value.___AN1;
    }

    inline int orientation(Point a, Point b, Point c) {
        const auto ab = b - a;
        const auto ac = c - a;
        const auto u = ab.value.___AN1 * ac.value.___AN2;
        const auto v = ab.value.___AN2 * ac.value.___AN1;
        const auto error = 16 * std::numeric_limits<long double>::epsilon() * (std::abs(u) + std::abs(v));
        return u - v > error ? 1 : (u - v < -error ? -1 : 0);
    }

    inline bool on_segment(Point a, Point b, Point p) {
        return orientation(a, b, p) == 0 &&
               p.value.___AN1 >= std::min(a.value.___AN1, b.value.___AN1) && p.value.___AN1 <= std::max(a.value.___AN1, b.value.___AN1) &&
               p.value.___AN2 >= std::min(a.value.___AN2, b.value.___AN2) && p.value.___AN2 <= std::max(a.value.___AN2, b.value.___AN2);
    }

    inline bool intersects(Point a, Point b, Point c, Point d) {
        const int ab_c = orientation(a, b, c), ab_d = orientation(a, b, d);
        const int cd_a = orientation(c, d, a), cd_b = orientation(c, d, b);
        return (ab_c * ab_d < 0 && cd_a * cd_b < 0) || on_segment(a, b, c) || on_segment(a, b, d) ||
               on_segment(c, d, a) || on_segment(c, d, b);
    }

    inline bool in_triangle(Point a, Point b, Point c, Point p) {
        const int winding = orientation(a, b, c);
        return winding != 0 && orientation(a, b, p) * winding >= 0 && orientation(b, c, p) * winding >= 0 &&
               orientation(c, a, p) * winding >= 0;
    }

    template<typename P>
    Point convert(const P& p) {
        return {static_cast<long double>(p.value.___AN1), static_cast<long double>(p.value.___AN2)};
    }

    inline void normalize(std::span<Point> points) {
        long double x_scale = 0, y_scale = 0;
        for (const auto p : points) {
            x_scale = std::max(x_scale, std::abs(p.value.___AN1));
            y_scale = std::max(y_scale, std::abs(p.value.___AN2));
        }
        const auto x_shift = x_scale > 0 ? -std::ilogb(x_scale) : 0;
        const auto y_shift = y_scale > 0 ? -std::ilogb(y_scale) : 0;
        for (auto& p : points) {
            p.value.___AN1 = std::scalbn(p.value.___AN1, x_shift);
            p.value.___AN2 = std::scalbn(p.value.___AN2, y_shift);
        }
    }
} // namespace AHO_NAMESPACE::detail::geometry2d
