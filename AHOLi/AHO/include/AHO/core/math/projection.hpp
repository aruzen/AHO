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

    template<HandedCoordinate handed = DEFAULT_COORDINATE_SYSTEM, DepthRange depth = DEFAULT_DEPTH_RANGE,
             ClipY clip_y = DEFAULT_CLIP_Y, VectorConvention convention = DEFAULT_VECTOR_CONVENTION, std::floating_point T>
    Mat4x4<T> make_orthographic(T left, T right, T bottom, T top, T near_plane, T far_plane);

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

    template<HandedCoordinate handed, DepthRange depth, ClipY clip_y, VectorConvention convention, std::floating_point T>
    Mat4x4<T> make_orthographic(T left, T right, T bottom, T top, T near_plane, T far_plane) {
        for (auto value : {left, right, bottom, top, near_plane, far_plane})
            if (!std::isfinite(value))
                throw std::invalid_argument("Orthographic bounds must be finite");
        if (!(left < right) || !(bottom < top) || !(near_plane < far_plane))
            throw std::invalid_argument("Orthographic bounds must be ascending");
        const T half_width = right / 2 - left / 2;
        const T half_height = top / 2 - bottom / 2;
        const T half_depth = far_plane / 2 - near_plane / 2;
        const T center_x = right / 2 + left / 2;
        const T center_y = top / 2 + bottom / 2;
        constexpr T y_sign = clip_y == ClipY::DOWN ? T{-1} : T{1};
        constexpr T z_sign = handed == HandedCoordinate::LEFT ? T{1} : T{-1};
        Mat4x4<T> result{{1 / half_width, 0, 0, -center_x / half_width},
                         {0, y_sign / half_height, 0, -y_sign * center_y / half_height},
                         {0, 0, 0, 0},
                         {0, 0, 0, 1}};
        if constexpr (depth == DepthRange::ZERO_TO_ONE) {
            result[2][2] = z_sign * T{.5} / half_depth;
            result[2][3] = -(near_plane / 2) / half_depth;
        } else {
            result[2][2] = z_sign / half_depth;
            result[2][3] = -(far_plane / 2 + near_plane / 2) / half_depth;
        }
        for (const auto& row : result)
            for (auto value : row)
                if (!std::isfinite(value))
                    throw std::invalid_argument("Orthographic matrix cannot be represented in the selected scalar type");
        return detail::with_vector_convention<convention>(result);
    }
} // namespace AHO_NAMESPACE::matrix
