#pragma once
#include <AHO/core/vector.hpp>
#include <AHO/core/point.hpp>
#include <cmath>
#include <stdexcept>

namespace AHO_NAMESPACE::x::scene {
    // AHO matrices serialized as GLSL column-major, RH/Y-up, Vulkan depth [0,1].
    struct Camera {
        d3::PointF eye{0, 0, 0};
        d3::VectorF direction{0, 0, -1};
        float verticalFov = 66, aspect = 1, nearPlane = .1f, farPlane = 1600;
        Mat4x4<float> projection() const {
            if (!std::isfinite(verticalFov) || verticalFov <= 0 || verticalFov >= 179 ||
                !std::isfinite(aspect) || aspect <= 0 || !std::isfinite(nearPlane) || nearPlane <= 0 ||
                !std::isfinite(farPlane) || farPlane <= nearPlane)
                throw std::invalid_argument("Invalid Vulkan camera projection");
            auto p = matrix::make_perspective(Degree{verticalFov}, aspect, nearPlane, farPlane);
            p[1][1] = -p[1][1];
            p[2][2] = farPlane / (nearPlane - farPlane);
            p[3][2] = nearPlane * farPlane / (nearPlane - farPlane);
            return p;
        }
        Mat4x4<float> view() const {
            const d3::VectorF up(0, 1, 0);
            const auto length = direction.length(), sideLength = direction.cross(up).length();
            if (!std::isfinite(length) || length < .00001f || sideLength < .00001f ||
                !std::isfinite(eye.value.x.value + eye.value.y.value + eye.value.z.value))
                throw std::invalid_argument("Invalid camera eye or direction");
            // Existing make_view deliberately has a 180-degree roll. Supplying -up
            // selects the conventional right-handed view without changing that API.
            return matrix::make_view(eye, eye + direction, -up);
        }
        Mat4x4<float> viewProjection() const {
            // Mat multiplication acts on the outer array as rows; GLSL sees those as columns.
            return view() * projection();
        }
    };
} // namespace AHO_NAMESPACE::x::scene
