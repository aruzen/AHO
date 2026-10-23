#pragma once
#include "functions.hpp"
#include "matrix_functions.hpp"

namespace AHO_NAMESPACE::matrix {
    template<VectorConvention convention = DEFAULT_VECTOR_CONVENTION, typename T, typename CI>
    constexpr Mat4x4<T> make_move(const Vector_<T, VSL_NAMESPACE::D3, CI>& offset);

    template<HandedCoordinate handed = DEFAULT_COORDINATE_SYSTEM, VectorConvention convention = DEFAULT_VECTOR_CONVENTION, std::floating_point T = float>
    Mat2x2<T> make_rotation(Radian angle);

    template<HandedCoordinate handed = DEFAULT_COORDINATE_SYSTEM, VectorConvention convention = DEFAULT_VECTOR_CONVENTION, std::floating_point T = float>
    Mat2x2<T> make_rotation(Degree angle);

    template<HandedCoordinate handed = DEFAULT_COORDINATE_SYSTEM, VectorConvention convention = DEFAULT_VECTOR_CONVENTION, std::floating_point T, typename CI>
    Mat4x4<T> make_rotation(Radian angle, Vector_<T, VSL_NAMESPACE::D3, CI> axis);

    template<HandedCoordinate handed = DEFAULT_COORDINATE_SYSTEM, VectorConvention convention = DEFAULT_VECTOR_CONVENTION, std::floating_point T, typename CI>
    Mat4x4<T> make_rotation(Degree angle, Vector_<T, VSL_NAMESPACE::D3, CI> axis);

    template<std::floating_point T>
    constexpr Mat4x4<T> make_scaling(const Vector_<T, VSL_NAMESPACE::D3>& value);

    // make_*は行列を生成する。こちらは既存行列へ変換を合成する。
    template<VectorConvention convention = DEFAULT_VECTOR_CONVENTION, std::floating_point T>
    Mat4x4<T> translate(const Mat4x4<T>& value, const Vector_<T, VSL_NAMESPACE::D3>& offset);

    template<HandedCoordinate handed = DEFAULT_COORDINATE_SYSTEM, VectorConvention convention = DEFAULT_VECTOR_CONVENTION, std::floating_point T>
    Mat4x4<T> rotate(const Mat4x4<T>& value, Radian angle, const Vector_<T, VSL_NAMESPACE::D3>& axis);

    template<HandedCoordinate handed = DEFAULT_COORDINATE_SYSTEM, VectorConvention convention = DEFAULT_VECTOR_CONVENTION, std::floating_point T>
    Mat4x4<T> rotate(const Mat4x4<T>& value, Degree angle, const Vector_<T, VSL_NAMESPACE::D3>& axis);

    template<VectorConvention convention = DEFAULT_VECTOR_CONVENTION, std::floating_point T>
    constexpr Mat4x4<T> scale(const Mat4x4<T>& value, const Vector_<T, VSL_NAMESPACE::D3>& factor);

    template<VectorConvention convention = DEFAULT_VECTOR_CONVENTION, std::floating_point T>
    constexpr std::array<T, 4> transform_homogeneous(const Mat4x4<T>& value, const std::array<T, 4>& point);

    // w=1で変換し、透視除算は行わない。
    template<VectorConvention convention = DEFAULT_VECTOR_CONVENTION, std::floating_point T>
    constexpr Point_<T, VSL_NAMESPACE::D3> transform_point(const Mat4x4<T>& value, const Point_<T, VSL_NAMESPACE::D3>& point);

    template<VectorConvention convention = DEFAULT_VECTOR_CONVENTION, std::floating_point T>
    constexpr Vector_<T, VSL_NAMESPACE::D3> transform_vector(const Mat4x4<T>& value, const Vector_<T, VSL_NAMESPACE::D3>& vector);

    template<VectorConvention convention = DEFAULT_VECTOR_CONVENTION, std::floating_point T>
    Point_<T, VSL_NAMESPACE::D3> project_point(const Mat4x4<T>& value, const Point_<T, VSL_NAMESPACE::D3>& point);

    template<VectorConvention convention = DEFAULT_VECTOR_CONVENTION, std::floating_point T>
    std::expected<Vector_<T, VSL_NAMESPACE::D3>, FailedCause> transform_normal(const Mat4x4<T>& value, const Vector_<T, VSL_NAMESPACE::D3>& normal);

    template<HandedCoordinate handed = DEFAULT_COORDINATE_SYSTEM, ViewOrientation orientation = DEFAULT_VIEW_ORIENTATION,
             VectorConvention convention = DEFAULT_VECTOR_CONVENTION, std::floating_point T, typename CI>
    Mat4x4<T> make_view(Point_<T, VSL_NAMESPACE::D3, CI> eye, Point_<T, VSL_NAMESPACE::D3, CI> target,
                        Vector_<T, VSL_NAMESPACE::D3, CI> up);

    template<VectorConvention convention, typename T, typename CI>
    constexpr Mat4x4<T> make_move(const Vector_<T, VSL_NAMESPACE::D3, CI>& offset) {
        const auto& a = offset.value;
        const Mat4x4<T> result{{1, 0, 0, a.___AN1}, {0, 1, 0, a.___AN2}, {0, 0, 1, a.___AN3}, {0, 0, 0, 1}};
        return detail::with_vector_convention<convention>(result);
    }

    template<HandedCoordinate handed, VectorConvention convention, std::floating_point T>
    Mat2x2<T> make_rotation(Radian angle) {
        if (!std::isfinite(angle.value))
            throw std::invalid_argument("Rotation angle must be finite");
        const T c = static_cast<T>(std::cos(angle.value));
        T s = static_cast<T>(std::sin(angle.value));
        if constexpr (handed == HandedCoordinate::LEFT)
            s = -s;
        const Mat2x2<T> result{{c, -s}, {s, c}};
        return detail::with_vector_convention<convention>(result);
    }

    template<HandedCoordinate handed, VectorConvention convention, std::floating_point T>
    Mat2x2<T> make_rotation(Degree angle) {
        return make_rotation<handed, convention, T>(static_cast<Radian>(angle));
    }

    template<HandedCoordinate handed, VectorConvention convention, std::floating_point T, typename CI>
    Mat4x4<T> make_rotation(Radian angle, Vector_<T, VSL_NAMESPACE::D3, CI> axis) {
        if (!std::isfinite(angle.value))
            throw std::invalid_argument("Rotation angle must be finite");
        const auto unit = math::normalized(axis);
        const auto& a = unit.value;
        const T x = a.___AN1, y = a.___AN2, z = a.___AN3;
        const T c = static_cast<T>(std::cos(angle.value));
        T s = static_cast<T>(std::sin(angle.value));
        if constexpr (handed == HandedCoordinate::LEFT)
            s = -s;
        const T d = 1 - c;
        const Mat4x4<T> result{{c + x * x * d, x * y * d - z * s, x * z * d + y * s, 0},
                               {y * x * d + z * s, c + y * y * d, y * z * d - x * s, 0},
                               {z * x * d - y * s, z * y * d + x * s, c + z * z * d, 0},
                               {0, 0, 0, 1}};
        return detail::with_vector_convention<convention>(result);
    }

    template<HandedCoordinate handed, VectorConvention convention, std::floating_point T, typename CI>
    Mat4x4<T> make_rotation(Degree angle, Vector_<T, VSL_NAMESPACE::D3, CI> axis) {
        return make_rotation<handed, convention>(static_cast<Radian>(angle), axis);
    }

    template<std::floating_point T>
    constexpr Mat4x4<T> make_scaling(const Vector_<T, VSL_NAMESPACE::D3>& value) {
        const auto a = math::to_array(value);
        return Mat4x4<T>{{a[0], 0, 0, 0}, {0, a[1], 0, 0}, {0, 0, a[2], 0}, {0, 0, 0, 1}};
    }

    template<VectorConvention convention, std::floating_point T>
    Mat4x4<T> translate(const Mat4x4<T>& value, const Vector_<T, VSL_NAMESPACE::D3>& offset) {
        if constexpr (convention == VectorConvention::COLUMN)
            return value * make_move<convention>(offset);
        else
            return make_move<convention>(offset) * value;
    }

    template<HandedCoordinate handed, VectorConvention convention, std::floating_point T>
    Mat4x4<T> rotate(const Mat4x4<T>& value, Radian angle, const Vector_<T, VSL_NAMESPACE::D3>& axis) {
        if constexpr (convention == VectorConvention::COLUMN)
            return value * make_rotation<handed, convention>(angle, axis);
        else
            return make_rotation<handed, convention>(angle, axis) * value;
    }

    template<HandedCoordinate handed, VectorConvention convention, std::floating_point T>
    Mat4x4<T> rotate(const Mat4x4<T>& value, Degree angle, const Vector_<T, VSL_NAMESPACE::D3>& axis) {
        return rotate<handed, convention>(value, static_cast<Radian>(angle), axis);
    }

    template<VectorConvention convention, std::floating_point T>
    constexpr Mat4x4<T> scale(const Mat4x4<T>& value, const Vector_<T, VSL_NAMESPACE::D3>& factor) {
        if constexpr (convention == VectorConvention::COLUMN)
            return value * make_scaling(factor);
        else
            return make_scaling(factor) * value;
    }

    template<VectorConvention convention, std::floating_point T>
    constexpr std::array<T, 4> transform_homogeneous(const Mat4x4<T>& value, const std::array<T, 4>& point) {
        std::array<T, 4> result{};
        for (std::size_t r = 0; r < 4; ++r)
            for (std::size_t c = 0; c < 4; ++c) {
                if constexpr (convention == VectorConvention::COLUMN)
                    result[r] += value[r][c] * point[c];
                else
                    result[r] += value[c][r] * point[c];
            }
        return result;
    }

    template<VectorConvention convention, std::floating_point T>
    constexpr Point_<T, VSL_NAMESPACE::D3> transform_point(const Mat4x4<T>& value, const Point_<T, VSL_NAMESPACE::D3>& point) {
        const auto a = math::to_array(point);
        const auto result = transform_homogeneous<convention>(value, {a[0], a[1], a[2], T{1}});
        return {result[0], result[1], result[2]};
    }

    template<VectorConvention convention, std::floating_point T>
    constexpr Vector_<T, VSL_NAMESPACE::D3> transform_vector(const Mat4x4<T>& value, const Vector_<T, VSL_NAMESPACE::D3>& vector) {
        const auto a = math::to_array(vector);
        const auto result = transform_homogeneous<convention>(value, {a[0], a[1], a[2], T{0}});
        return {result[0], result[1], result[2]};
    }

    template<VectorConvention convention, std::floating_point T>
    Point_<T, VSL_NAMESPACE::D3> project_point(const Mat4x4<T>& value, const Point_<T, VSL_NAMESPACE::D3>& point) {
        const auto a = math::to_array(point);
        const auto result = transform_homogeneous<convention>(value, {a[0], a[1], a[2], T{1}});
        if (!std::isfinite(result[3]) || result[3] == 0)
            throw std::invalid_argument("Projected point requires finite nonzero w");
        return {result[0] / result[3], result[1] / result[3], result[2] / result[3]};
    }

    template<VectorConvention convention, std::floating_point T>
    std::expected<Vector_<T, VSL_NAMESPACE::D3>, FailedCause> transform_normal(const Mat4x4<T>& value, const Vector_<T, VSL_NAMESPACE::D3>& normal) {
        const auto inverse = inverse_affine<convention>(value);
        if (!inverse)
            return std::unexpected(inverse.error());
        return transform_vector<convention>(transpose(*inverse), normal);
    }

    template<HandedCoordinate handed, ViewOrientation orientation, VectorConvention convention, std::floating_point T, typename CI>
    Mat4x4<T> make_view(Point_<T, VSL_NAMESPACE::D3, CI> eye, Point_<T, VSL_NAMESPACE::D3, CI> target,
                        Vector_<T, VSL_NAMESPACE::D3, CI> up) {
        for (auto element : math::to_array(eye))
            if (!std::isfinite(element))
                throw std::invalid_argument("Camera position must be finite");
        auto z = math::normalized(target - eye);
        if constexpr (handed == HandedCoordinate::RIGHT)
            z = -z;
        auto right = math::normalized(math::normalized(up).cross(z));
        auto vertical = z.cross(right);
        if constexpr (orientation == ViewOrientation::ROLL_180) {
            right = -right;
            vertical = -vertical;
        }
        const Vector_<T, VSL_NAMESPACE::D3, CI> position{eye.value};
        const auto s = math::to_array(right), u = math::to_array(vertical), f = math::to_array(z);
        const Mat4x4<T> result{{s[0], s[1], s[2], -right.dot(position)},
                               {u[0], u[1], u[2], -vertical.dot(position)},
                               {f[0], f[1], f[2], -z.dot(position)},
                               {0, 0, 0, 1}};
        return detail::with_vector_convention<convention>(result);
    }
} // namespace AHO_NAMESPACE::matrix
