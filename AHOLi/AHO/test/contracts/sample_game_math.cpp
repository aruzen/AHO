#include <AHO/core/math/projection.hpp>

#ifdef GLM_VERSION
#error AHO math must not include GLM
#endif

namespace {
    struct Vertex {
        std::array<float, 3> position, normal, color;
        float material;
    };
    struct Scene {
        std::array<float, 16> view_projection;
        std::array<float, 4> eye_time, settings, forward, right;
    };
    static_assert(sizeof(Vertex) == 40 && offsetof(Vertex, material) == 36);
    static_assert(sizeof(Scene) == 128);

    Scene scene(float time, const aho::d3::PointF& eye, const aho::d3::VectorF& direction);
    std::array<float, 16> model(const aho::d2::PointF& position, aho::Radian yaw);
    Vertex triangle_vertex(const aho::d3::PointF& a, const aho::d3::PointF& b, const aho::d3::PointF& c, float altitude);
    float walk_distance(const aho::d2::PointF& first, const aho::d2::PointF& second);

    Scene scene(float time, const aho::d3::PointF& eye, const aho::d3::VectorF& direction) {
        const auto forward = aho::math::normalized(direction);
        const auto right = aho::math::normalized(forward.cross(aho::d3::VectorF{0, 1, 0}));
        const auto view = aho::matrix::make_view<aho::HandedCoordinate::RIGHT>(eye, eye + direction, aho::d3::VectorF{0, 1, 0});
        const auto projection = aho::matrix::make_perspective<aho::HandedCoordinate::RIGHT, aho::DepthRange::ZERO_TO_ONE, aho::ClipY::DOWN>(aho::Degree{66}, 1.6f, .1f, 1600.f);
        const auto e = aho::math::to_array(eye), f = aho::math::to_array(forward), r = aho::math::to_array(right);
        return {aho::matrix::to_array<aho::MatrixLayout::COLUMN_MAJOR>(projection * view),
                {e[0], e[1], e[2], time},
                {0, 1.6f, 1, 0},
                {f[0], f[1], f[2], 800},
                {r[0], r[1], r[2], 1280}};
    }

    std::array<float, 16> model(const aho::d2::PointF& position, aho::Radian yaw) {
        const auto p = aho::math::to_array(position);
        auto matrix = aho::matrix::translate(aho::matrix::make_identity<float, 4>(), aho::d3::VectorF{p[0], 2, p[1]});
        matrix = aho::matrix::rotate<aho::HandedCoordinate::RIGHT>(matrix, yaw, aho::d3::VectorF{0, 1, 0});
        matrix = aho::matrix::scale(matrix, aho::d3::VectorF{.014f, .014f, .014f});
        return aho::matrix::to_array<aho::MatrixLayout::COLUMN_MAJOR>(matrix);
    }

    Vertex triangle_vertex(const aho::d3::PointF& a, const aho::d3::PointF& b, const aho::d3::PointF& c, float altitude) {
        const auto normal = aho::math::normalized((b - a).cross(c - a));
        const auto snow = aho::math::smoothstep(60.f, 145.f, altitude);
        const auto color = aho::math::lerp(aho::d3::VectorF{.14f, .35f, .27f}, aho::d3::VectorF{.72f, .84f, .8f}, snow);
        return {aho::math::to_array(a), aho::math::to_array(normal), aho::math::to_array(color), 0};
    }

    float walk_distance(const aho::d2::PointF& first, const aho::d2::PointF& second) {
        return aho::math::distance(first, second);
    }
} // namespace
