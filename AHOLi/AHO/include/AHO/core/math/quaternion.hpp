#pragma once
#include "transform.hpp"

namespace AHO_NAMESPACE {
    // 回転を表す単位Quaternion。成分はglTFと同じx,y,z,w順で取得する。
    template<std::floating_point T = float, HandedCoordinate handed = DEFAULT_COORDINATE_SYSTEM>
    class Quaternion {
        std::array<T, 4> _components;

        constexpr explicit Quaternion(std::array<T, 4> components);

    public:
        constexpr Quaternion();

        // 有限で非ゼロの成分を正規化する。入力のスケールには依存しない。
        static Quaternion FromComponents(T x, T y, T z, T w);
        static Quaternion FromAxisAngle(Radian angle, const Vector_<T, VSL_NAMESPACE::D3>& axis);
        static Quaternion FromAxisAngle(Degree angle, const Vector_<T, VSL_NAMESPACE::D3>& axis);

        constexpr std::array<T, 4> components() const;
        constexpr Quaternion inverse() const;
        // q1*q2はq2→q1の順で回転する。
        Quaternion operator*(const Quaternion& other) const;
        constexpr Vector_<T, VSL_NAMESPACE::D3> rotate(const Vector_<T, VSL_NAMESPACE::D3>& vector) const;
        template<VectorConvention convention = DEFAULT_VECTOR_CONVENTION>
        constexpr Mat4x4<T> to_matrix() const;
    };

    using QuaternionF = Quaternion<float>;
    using QuaternionD = Quaternion<double>;

    namespace math {
        // 経路の選択はtemplate引数で指定する。有限のamountを受け取り、範囲外は外挿する。
        template<InterpolationPath path = DEFAULT_INTERPOLATION_PATH, std::floating_point T, HandedCoordinate handed>
        Quaternion<T, handed> slerp(const Quaternion<T, handed>& first, const Quaternion<T, handed>& second, T amount);
    } // namespace math

    namespace matrix {
        // 拡縮→回転→平行移動を合成する。ゼロ・負の拡縮も許可する。
        template<VectorConvention convention = DEFAULT_VECTOR_CONVENTION, std::floating_point T, HandedCoordinate handed>
        Mat4x4<T> make_trs(const Vector_<T, VSL_NAMESPACE::D3>& translation, const Quaternion<T, handed>& rotation,
                           const Vector_<T, VSL_NAMESPACE::D3>& scaling);
    } // namespace matrix

    template<std::floating_point T, HandedCoordinate handed>
    constexpr Quaternion<T, handed>::Quaternion(std::array<T, 4> components) : _components(components) {}

    template<std::floating_point T, HandedCoordinate handed>
    constexpr Quaternion<T, handed>::Quaternion() : _components{0, 0, 0, 1} {}

    template<std::floating_point T, HandedCoordinate handed>
    Quaternion<T, handed> Quaternion<T, handed>::FromComponents(T x, T y, T z, T w) {
        if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z) || !std::isfinite(w))
            throw std::invalid_argument("Quaternion components must be finite");
        const auto scale = std::max({std::abs(x), std::abs(y), std::abs(z), std::abs(w)});
        if (scale == 0)
            throw std::invalid_argument("Quaternion must be nonzero");
        x /= scale;
        y /= scale;
        z /= scale;
        w /= scale;
        const auto length = std::sqrt(x * x + y * y + z * z + w * w);
        return Quaternion(std::array<T, 4>{x / length, y / length, z / length, w / length});
    }

    template<std::floating_point T, HandedCoordinate handed>
    Quaternion<T, handed> Quaternion<T, handed>::FromAxisAngle(Radian angle, const Vector_<T, VSL_NAMESPACE::D3>& axis) {
        if (!std::isfinite(angle.value))
            throw std::invalid_argument("Quaternion angle must be finite");
        const auto values = math::to_array(axis);
        for (auto value : values)
            if (!std::isfinite(value))
                throw std::invalid_argument("Quaternion axis must be finite");
        const auto scale = std::max({std::abs(values[0]), std::abs(values[1]), std::abs(values[2])});
        if (scale == 0)
            throw std::invalid_argument("Quaternion axis must be nonzero");
        const auto unit = math::to_array(math::normalized(axis / scale));
        T sine = static_cast<T>(std::sin(angle.value / 2));
        if constexpr (handed == HandedCoordinate::LEFT)
            sine = -sine;
        const T cosine = static_cast<T>(std::cos(angle.value / 2));
        return FromComponents(unit[0] * sine, unit[1] * sine, unit[2] * sine, cosine);
    }

    template<std::floating_point T, HandedCoordinate handed>
    Quaternion<T, handed> Quaternion<T, handed>::FromAxisAngle(Degree angle, const Vector_<T, VSL_NAMESPACE::D3>& axis) {
        return FromAxisAngle(static_cast<Radian>(angle), axis);
    }

    template<std::floating_point T, HandedCoordinate handed>
    constexpr std::array<T, 4> Quaternion<T, handed>::components() const {
        return _components;
    }

    template<std::floating_point T, HandedCoordinate handed>
    constexpr Quaternion<T, handed> Quaternion<T, handed>::inverse() const {
        const auto& q = _components;
        return Quaternion(std::array<T, 4>{-q[0], -q[1], -q[2], q[3]});
    }

    template<std::floating_point T, HandedCoordinate handed>
    Quaternion<T, handed> Quaternion<T, handed>::operator*(const Quaternion& other) const {
        const auto& a = _components;
        const auto& b = other._components;
        return FromComponents(a[3] * b[0] + a[0] * b[3] + a[1] * b[2] - a[2] * b[1],
                              a[3] * b[1] - a[0] * b[2] + a[1] * b[3] + a[2] * b[0],
                              a[3] * b[2] + a[0] * b[1] - a[1] * b[0] + a[2] * b[3],
                              a[3] * b[3] - a[0] * b[0] - a[1] * b[1] - a[2] * b[2]);
    }

    template<std::floating_point T, HandedCoordinate handed>
    constexpr Vector_<T, VSL_NAMESPACE::D3> Quaternion<T, handed>::rotate(const Vector_<T, VSL_NAMESPACE::D3>& vector) const {
        const auto& q = _components;
        const Vector_<T, VSL_NAMESPACE::D3> axis{q[0], q[1], q[2]};
        const auto cross = axis.cross(vector);
        return vector + cross * (T{2} * q[3]) + axis.cross(cross) * T{2};
    }

    template<std::floating_point T, HandedCoordinate handed>
    template<VectorConvention convention>
    constexpr Mat4x4<T> Quaternion<T, handed>::to_matrix() const {
        const auto& q = _components;
        const T x = q[0], y = q[1], z = q[2], w = q[3];
        const Mat4x4<T> result{{1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w), 0},
                               {2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w), 0},
                               {2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y), 0},
                               {0, 0, 0, 1}};
        return matrix::detail::with_vector_convention<convention>(result);
    }

    namespace math {
        template<InterpolationPath path, std::floating_point T, HandedCoordinate handed>
        Quaternion<T, handed> slerp(const Quaternion<T, handed>& first, const Quaternion<T, handed>& second, T amount) {
            if (!std::isfinite(amount))
                throw std::invalid_argument("Quaternion interpolation amount must be finite");
            if (amount == 0)
                return first;
            if (amount == 1)
                return second;
            const auto a = first.components();
            auto b = second.components();
            long double cosine = 0;
            for (std::size_t i = 0; i < 4; ++i)
                cosine += static_cast<long double>(a[i]) * b[i];
            if constexpr (path == InterpolationPath::SHORTEST) {
                if (cosine < 0) {
                    for (auto& component : b)
                        component = -component;
                    cosine = -cosine;
                }
            } else {
                if (cosine < -1 + 8 * std::numeric_limits<T>::epsilon())
                    throw std::invalid_argument("Direct interpolation of antipodal quaternions has no unique path");
            }
            cosine = std::clamp(cosine, -1.L, 1.L);
            std::array<T, 4> result{};
            if (cosine > 1 - 8 * std::numeric_limits<T>::epsilon()) {
                result = math::lerp(a, b, amount);
            } else {
                const auto angle = std::acos(cosine);
                const auto denominator = std::sin(angle);
                const auto weight_first = std::sin((1.L - amount) * angle) / denominator;
                const auto weight_second = std::sin(static_cast<long double>(amount) * angle) / denominator;
                for (std::size_t i = 0; i < 4; ++i)
                    result[i] = static_cast<T>(weight_first * a[i] + weight_second * b[i]);
            }
            return Quaternion<T, handed>::FromComponents(result[0], result[1], result[2], result[3]);
        }
    } // namespace math

    namespace matrix {
        template<VectorConvention convention, std::floating_point T, HandedCoordinate handed>
        Mat4x4<T> make_trs(const Vector_<T, VSL_NAMESPACE::D3>& translation, const Quaternion<T, handed>& rotation,
                           const Vector_<T, VSL_NAMESPACE::D3>& scaling) {
            const auto offset = math::to_array(translation), factor = math::to_array(scaling);
            auto result = rotation.template to_matrix<VectorConvention::COLUMN>();
            for (std::size_t r = 0; r < 3; ++r) {
                if (!std::isfinite(offset[r]) || !std::isfinite(factor[r]))
                    throw std::invalid_argument("TRS translation and scaling must be finite");
                for (std::size_t c = 0; c < 3; ++c)
                    result[r][c] *= factor[c];
                result[r][3] = offset[r];
            }
            return detail::with_vector_convention<convention>(result);
        }
    } // namespace matrix
} // namespace AHO_NAMESPACE
