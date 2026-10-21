#pragma once
#include "polygon.hpp"
#include "detail/geometry2d.hpp"
#include <expected>
#include <numeric>

namespace AHO_NAMESPACE {
    enum class TriangulationFailedCause {
        TOO_FEW_VERTICES,
        NON_FINITE_COORDINATE,
        DUPLICATE_VERTEX,
        DEGENERATE_CONTOUR,
        SELF_INTERSECTION,
        INDEX_OVERFLOW,
        NO_EAR
    };

    // 点列は輪郭順。終点に始点を繰り返す形式も受け付ける。穴は扱わない。
    template<typename Size = std::size_t, std::ranges::input_range Range>
    requires concepts::is_point<std::ranges::range_value_t<Range>> &&
             std::same_as<typename std::ranges::range_value_t<Range>::dimension, VSL_NAMESPACE::D2> &&
             std::is_arithmetic_v<typename std::ranges::range_value_t<Range>::element_type> &&
             std::unsigned_integral<Size> && (!std::same_as<Size, bool>)
    [[nodiscard]] auto triangulate(Range&& contour);

    template<typename Size, std::ranges::input_range Range>
    requires concepts::is_point<std::ranges::range_value_t<Range>> &&
             std::same_as<typename std::ranges::range_value_t<Range>::dimension, VSL_NAMESPACE::D2> &&
             std::is_arithmetic_v<typename std::ranges::range_value_t<Range>::element_type> &&
             std::unsigned_integral<Size> && (!std::same_as<Size, bool>)
    auto triangulate(Range&& contour) {
        using Input = std::ranges::range_value_t<Range>;
        using Mesh = Polygon<typename Input::element_type, VSL_NAMESPACE::D2, typename Input::coordinate_info, Size>;
        using P = typename Mesh::PointType;
        using Error = TriangulationFailedCause;
        using Result = std::expected<Mesh, Error>;
        namespace g = detail::geometry2d;
        std::vector<P> points;
        std::vector<g::Point> normalized;
        for (const auto& point : contour) {
            if (!points.empty() && points.size() - 1 > std::numeric_limits<Size>::max())
                return Result(std::unexpected(Error::INDEX_OVERFLOW));
            const auto p = g::convert(point);
            if (!std::isfinite(p.value.___AN1) || !std::isfinite(p.value.___AN2))
                return Result(std::unexpected(Error::NON_FINITE_COORDINATE));
            points.push_back(point);
            normalized.push_back(p);
        }
        if (points.size() > 1 && normalized.front() == normalized.back()) {
            points.pop_back();
            normalized.pop_back();
        }
        if (!points.empty() && points.size() - 1 > std::numeric_limits<Size>::max())
            return Result(std::unexpected(Error::INDEX_OVERFLOW));
        if (points.size() < 3)
            return Result(std::unexpected(Error::TOO_FEW_VERTICES));
        for (std::size_t i = 0; i < points.size(); ++i)
            for (std::size_t j = 0; j < i; ++j)
                if (normalized[i] == normalized[j])
                    return Result(std::unexpected(Error::DUPLICATE_VERTEX));
        g::normalize(normalized);
        const auto n = points.size();
        for (std::size_t i = 0; i < n; ++i)
            for (std::size_t j = i + 1; j < n; ++j) {
                if (j == i + 1 || (i == 0 && j == n - 1))
                    continue;
                if (g::intersects(normalized[i], normalized[(i + 1) % n], normalized[j], normalized[(j + 1) % n]))
                    return Result(std::unexpected(Error::SELF_INTERSECTION));
            }
        std::vector<std::size_t> remaining(n);
        std::iota(remaining.begin(), remaining.end(), 0);
        // 同一直線上で前後の点の間にある頂点だけを分割対象から外す。
        bool simplified = true;
        while (simplified && remaining.size() > 3) {
            simplified = false;
            for (std::size_t i = 0; i < remaining.size(); ++i) {
                const auto a = normalized[remaining[(i + remaining.size() - 1) % remaining.size()]];
                const auto b = normalized[remaining[i]];
                const auto c = normalized[remaining[(i + 1) % remaining.size()]];
                if (g::on_segment(a, c, b)) {
                    remaining.erase(remaining.begin() + i);
                    simplified = true;
                    break;
                }
                if (g::orientation(a, b, c) == 0)
                    return Result(std::unexpected(Error::DEGENERATE_CONTOUR));
            }
        }
        long double twice_area = 0;
        for (std::size_t i = 1; i + 1 < remaining.size(); ++i)
            twice_area += g::cross(normalized[remaining[0]], normalized[remaining[i]], normalized[remaining[i + 1]]);
        if (twice_area == 0 || g::orientation(normalized[remaining[0]], normalized[remaining[1]], normalized[remaining[2]]) == 0)
            return Result(std::unexpected(Error::DEGENERATE_CONTOUR));
        const int winding = twice_area > 0 ? 1 : -1;
        std::vector<IndexedTriangle<Size>> triangles;
        triangles.reserve(remaining.size() - 2);
        while (remaining.size() > 3) {
            bool clipped = false;
            for (std::size_t i = 0; i < remaining.size(); ++i) {
                const auto a = remaining[(i + remaining.size() - 1) % remaining.size()];
                const auto b = remaining[i], c = remaining[(i + 1) % remaining.size()];
                if (g::orientation(normalized[a], normalized[b], normalized[c]) != winding)
                    continue;
                bool occupied = false;
                for (const auto p : remaining)
                    if (p != a && p != b && p != c && g::in_triangle(normalized[a], normalized[b], normalized[c], normalized[p])) {
                        occupied = true;
                        break;
                    }
                if (occupied)
                    continue;
                triangles.push_back({{static_cast<Size>(a)}, {static_cast<Size>(b)}, {static_cast<Size>(c)}});
                remaining.erase(remaining.begin() + i);
                clipped = true;
                break;
            }
            if (!clipped)
                return Result(std::unexpected(Error::NO_EAR));
        }
        triangles.push_back({{static_cast<Size>(remaining[0])}, {static_cast<Size>(remaining[1])}, {static_cast<Size>(remaining[2])}});
        return Result(Mesh(points, triangles));
    }
} // namespace AHO_NAMESPACE
