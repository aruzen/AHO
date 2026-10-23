#pragma once
#include "transform.hpp"
#include <numbers>

namespace AHO_NAMESPACE::matrix {
    template<HandedCoordinate handed = DEFAULT_COORDINATE_SYSTEM, DepthRange depth = DEFAULT_DEPTH_RANGE,
             ClipY clip_y = DEFAULT_CLIP_Y, VectorConvention convention = DEFAULT_VECTOR_CONVENTION, std::floating_point T>
    Mat4x4<T> make_perspective(Radian fov, T aspect, T near_plane, T far_plane);

    template<HandedCoordinate handed = DEFAULT_COORDINATE_SYSTEM, DepthRange depth = DEFAULT_DEPTH_RANGE,
             ClipY clip_y = DEFAULT_CLIP_Y, VectorConvention convention = DEFAULT_VECTOR_CONVENTION, std::floating_point T>
    Mat4x4<T> make_perspective(Degree fov, T aspect, T near_plane, T far_plane);

    template<HandedCoordinate handed, DepthRange depth, ClipY clip_y, VectorConvention convention, std::floating_point T>
    Mat4x4<T> make_perspective(Radian fov, T aspect, T near_plane, T far_plane) {
        if (!std::isfinite(fov.value) || fov.value <= 0 || fov.value >= std::numbers::pi_v<long double> ||
            !std::isfinite(aspect) || aspect <= 0 || !std::isfinite(near_plane) || near_plane <= 0 ||
            !std::isfinite(far_plane) || far_plane <= near_plane)
            throw std::invalid_argument("Invalid perspective parameters");
        const T focal = static_cast<T>(1 / std::tan(fov.value / 2));
        const T ratio = near_plane / (far_plane - near_plane);
        const T far_ratio = far_plane / (far_plane - near_plane);
        constexpr T sign = handed == HandedCoordinate::LEFT ? T{1} : T{-1};
        Mat4x4<T> result{};
        result[0][0] = focal / aspect;
        result[1][1] = clip_y == ClipY::DOWN ? -focal : focal;
        result[3][2] = sign;
        if constexpr (depth == DepthRange::ZERO_TO_ONE) {
            result[2][2] = sign * far_ratio;
            result[2][3] = -near_plane * far_ratio;
        } else {
            result[2][2] = sign * (1 + 2 * ratio);
            result[2][3] = -(near_plane * far_ratio) * 2;
        }
        return detail::with_vector_convention<convention>(result);
    }

    template<HandedCoordinate handed, DepthRange depth, ClipY clip_y, VectorConvention convention, std::floating_point T>
    Mat4x4<T> make_perspective(Degree fov, T aspect, T near_plane, T far_plane) {
        return make_perspective<handed, depth, clip_y, convention>(static_cast<Radian>(fov), aspect, near_plane, far_plane);
    }
} // namespace AHO_NAMESPACE::matrix
