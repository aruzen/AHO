#pragma once
#include "functions.hpp"
#include <numbers>

namespace AHO_NAMESPACE::matrix {
    // このヘッダーはmat[row][column]と列ベクトルを使う。A*BはB→Aの順で作用する。
    // GLSL/glTFのcolumn-major保存形式はto/from_column_majorで明示変換する。
    template<typename T, std::size_t rows, std::size_t columns>
    constexpr std::array<T, rows * columns> to_column_major(const Mat<T, rows, columns>& value);

    template<typename T, std::size_t rows, std::size_t columns>
    constexpr Mat<T, rows, columns> from_column_major(const std::array<T, rows * columns>& value);

    template<typename T, std::size_t rows, std::size_t columns>
    constexpr Mat<T, columns, rows> transpose(const Mat<T, rows, columns>& value);

    template<std::floating_point T>
    constexpr Mat4x4<T> make_scaling(const Vector_<T, VSL_NAMESPACE::D3>& value);

    // GLMと同じく右側へ合成する。例えばtranslate→rotate→scaleはT*R*Sになる。
    template<std::floating_point T>
    Mat4x4<T> translate(const Mat4x4<T>& value, const Vector_<T, VSL_NAMESPACE::D3>& offset);

    template<std::floating_point T>
    Mat4x4<T> rotate(const Mat4x4<T>& value, Radian angle, const Vector_<T, VSL_NAMESPACE::D3>& axis);

    template<std::floating_point T>
    Mat4x4<T> rotate(const Mat4x4<T>& value, Degree angle, const Vector_<T, VSL_NAMESPACE::D3>& axis);

    template<std::floating_point T>
    constexpr Mat4x4<T> scale(const Mat4x4<T>& value, const Vector_<T, VSL_NAMESPACE::D3>& factor);

    template<std::floating_point T>
    constexpr std::array<T, 4> transform_homogeneous(const Mat4x4<T>& value, const std::array<T, 4>& point);

    // w=1で変換し、透視除算は行わない。射影後の座標はproject_pointで得る。
    template<std::floating_point T>
    constexpr Point_<T, VSL_NAMESPACE::D3> transform_point(const Mat4x4<T>& value, const Point_<T, VSL_NAMESPACE::D3>& point);

    template<std::floating_point T>
    constexpr Vector_<T, VSL_NAMESPACE::D3> transform_vector(const Mat4x4<T>& value, const Vector_<T, VSL_NAMESPACE::D3>& vector);

    template<std::floating_point T>
    Point_<T, VSL_NAMESPACE::D3> project_point(const Mat4x4<T>& value, const Point_<T, VSL_NAMESPACE::D3>& point);

    // 非一様拡縮に対応する逆転置。正規化は呼び出し側で行う。
    template<std::floating_point T>
    std::expected<Vector_<T, VSL_NAMESPACE::D3>, FailedCause> transform_normal(const Mat4x4<T>& value, const Vector_<T, VSL_NAMESPACE::D3>& normal);

    // 従来のmake_viewのrollを維持し、通常の右手系viewを別名で提供する。
    template<std::floating_point T>
    Mat4x4<T> make_look_at(const Point_<T, VSL_NAMESPACE::D3>& eye, const Point_<T, VSL_NAMESPACE::D3>& target, const Vector_<T, VSL_NAMESPACE::D3>& up);

    // 右手系、視線は-Z、near/farのdepthは0/1。Vulkanではflip_y=trueを指定できる。
    template<std::floating_point T>
    Mat4x4<T> make_perspective_zero_to_one(Radian fov, T aspect, T near_plane, T far_plane, bool flip_y = false);

    template<std::floating_point T>
    Mat4x4<T> make_perspective_zero_to_one(Degree fov, T aspect, T near_plane, T far_plane, bool flip_y = false);

    template<typename T, std::size_t rows, std::size_t columns>
    constexpr std::array<T, rows * columns> to_column_major(const Mat<T, rows, columns>& value) {
        std::array<T, rows * columns> result{};
        for (std::size_t r = 0; r < rows; ++r)
            for (std::size_t c = 0; c < columns; ++c)
                result[c * rows + r] = value[r][c];
        return result;
    }

    template<typename T, std::size_t rows, std::size_t columns>
    constexpr Mat<T, rows, columns> from_column_major(const std::array<T, rows * columns>& value) {
        Mat<T, rows, columns> result{};
        for (std::size_t r = 0; r < rows; ++r)
            for (std::size_t c = 0; c < columns; ++c)
                result[r][c] = value[c * rows + r];
        return result;
    }

    template<typename T, std::size_t rows, std::size_t columns>
    constexpr Mat<T, columns, rows> transpose(const Mat<T, rows, columns>& value) {
        Mat<T, columns, rows> result{};
        for (std::size_t r = 0; r < rows; ++r)
            for (std::size_t c = 0; c < columns; ++c)
                result[c][r] = value[r][c];
        return result;
    }

    template<std::floating_point T>
    constexpr Mat4x4<T> make_scaling(const Vector_<T, VSL_NAMESPACE::D3>& value) {
        const auto a = math::to_array(value);
        return Mat4x4<T>{{a[0], 0, 0, 0}, {0, a[1], 0, 0}, {0, 0, a[2], 0}, {0, 0, 0, 1}};
    }

    template<std::floating_point T>
    Mat4x4<T> translate(const Mat4x4<T>& value, const Vector_<T, VSL_NAMESPACE::D3>& offset) {
        // make_moveはinitializer-list factoryの列指定規約を使うため、明示的に変換する。
        return value * transpose(make_move<T>(offset));
    }

    template<std::floating_point T>
    Mat4x4<T> rotate(const Mat4x4<T>& value, Radian angle, const Vector_<T, VSL_NAMESPACE::D3>& axis) {
        if (!std::isfinite(angle.value))
            throw std::invalid_argument("Rotation angle must be finite");
        // 既存factoryにも正規化があるため、ここでは入力の有効性だけを確認する。
        const auto length = axis.length();
        if (!std::isfinite(length) || length <= 0)
            throw std::invalid_argument("Rotation axis must be finite and nonzero");
        return value * make_rotation<T>(angle, axis);
    }

    template<std::floating_point T>
    Mat4x4<T> rotate(const Mat4x4<T>& value, Degree angle, const Vector_<T, VSL_NAMESPACE::D3>& axis) {
        return rotate(value, static_cast<Radian>(angle), axis);
    }

    template<std::floating_point T>
    constexpr Mat4x4<T> scale(const Mat4x4<T>& value, const Vector_<T, VSL_NAMESPACE::D3>& factor) {
        return value * make_scaling(factor);
    }

    template<std::floating_point T>
    constexpr std::array<T, 4> transform_homogeneous(const Mat4x4<T>& value, const std::array<T, 4>& point) {
        std::array<T, 4> result{};
        for (std::size_t r = 0; r < 4; ++r)
            for (std::size_t c = 0; c < 4; ++c)
                result[r] += value[r][c] * point[c];
        return result;
    }

    template<std::floating_point T>
    constexpr Point_<T, VSL_NAMESPACE::D3> transform_point(const Mat4x4<T>& value, const Point_<T, VSL_NAMESPACE::D3>& point) {
        const auto a = math::to_array(point);
        const auto result = transform_homogeneous(value, {a[0], a[1], a[2], T{1}});
        return {result[0], result[1], result[2]};
    }

    template<std::floating_point T>
    constexpr Vector_<T, VSL_NAMESPACE::D3> transform_vector(const Mat4x4<T>& value, const Vector_<T, VSL_NAMESPACE::D3>& vector) {
        const auto a = math::to_array(vector);
        const auto result = transform_homogeneous(value, {a[0], a[1], a[2], T{0}});
        return {result[0], result[1], result[2]};
    }

    template<std::floating_point T>
    Point_<T, VSL_NAMESPACE::D3> project_point(const Mat4x4<T>& value, const Point_<T, VSL_NAMESPACE::D3>& point) {
        const auto a = math::to_array(point);
        const auto result = transform_homogeneous(value, {a[0], a[1], a[2], T{1}});
        if (!std::isfinite(result[3]) || result[3] == 0)
            throw std::invalid_argument("Projected point requires finite nonzero w");
        return {result[0] / result[3], result[1] / result[3], result[2] / result[3]};
    }

    template<std::floating_point T>
    std::expected<Vector_<T, VSL_NAMESPACE::D3>, FailedCause> transform_normal(const Mat4x4<T>& value, const Vector_<T, VSL_NAMESPACE::D3>& normal) {
        const auto inverse = inverse_affine(value);
        if (!inverse)
            return std::unexpected(inverse.error());
        return transform_vector(transpose(*inverse), normal);
    }

    template<std::floating_point T>
    Mat4x4<T> make_look_at(const Point_<T, VSL_NAMESPACE::D3>& eye, const Point_<T, VSL_NAMESPACE::D3>& target, const Vector_<T, VSL_NAMESPACE::D3>& up) {
        for (auto element : math::to_array(eye))
            if (!std::isfinite(element))
                throw std::invalid_argument("Camera position must be finite");
        const auto forward = math::normalized(target - eye);
        const auto right = math::normalized(forward.cross(math::normalized(up)));
        const auto vertical = right.cross(forward);
        const Vector_<T, VSL_NAMESPACE::D3> position{eye.value};
        const auto f = math::to_array(forward), s = math::to_array(right), u = math::to_array(vertical);
        return Mat4x4<T>{{s[0], s[1], s[2], -right.dot(position)},
                         {u[0], u[1], u[2], -vertical.dot(position)},
                         {-f[0], -f[1], -f[2], forward.dot(position)},
                         {0, 0, 0, 1}};
    }

    template<std::floating_point T>
    Mat4x4<T> make_perspective_zero_to_one(Radian fov, T aspect, T near_plane, T far_plane, bool flip_y) {
        if (!std::isfinite(fov.value) || fov.value <= 0 || fov.value >= std::numbers::pi_v<long double> ||
            !std::isfinite(aspect) || aspect <= 0 || !std::isfinite(near_plane) || near_plane <= 0 ||
            !std::isfinite(far_plane) || far_plane <= near_plane)
            throw std::invalid_argument("Invalid perspective parameters");
        const T focal = static_cast<T>(1 / std::tan(fov.value / 2));
        const T depth = far_plane / (near_plane - far_plane);
        // near*farを先に計算せず、中間のoverflowを避ける。
        return Mat4x4<T>{{focal / aspect, 0, 0, 0},
                         {0, flip_y ? -focal : focal, 0, 0},
                         {0, 0, depth, near_plane * depth},
                         {0, 0, -1, 0}};
    }

    template<std::floating_point T>
    Mat4x4<T> make_perspective_zero_to_one(Degree fov, T aspect, T near_plane, T far_plane, bool flip_y) {
        return make_perspective_zero_to_one(static_cast<Radian>(fov), aspect, near_plane, far_plane, flip_y);
    }
} // namespace AHO_NAMESPACE::matrix
