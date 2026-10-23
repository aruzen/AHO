#pragma once
#include "transform.hpp"

namespace AHO_NAMESPACE {
    // xyzwはw + xi + yj + zkに対応する。構築・積で正規化しない。
    template<typename T = float>
    struct Quaternion : d4::Vector<T> {
        using Base = d4::Vector<T>;
        constexpr Quaternion() = default;
        constexpr Quaternion(T x, T y, T z, T w) : Base(x, y, z, w) {}
        template<typename U>
        requires std::constructible_from<T, const U&>
        constexpr explicit Quaternion(const d4::Vector<U>& v)
            : Base(T(v.value.___AN1), T(v.value.___AN2), T(v.value.___AN3), T(v.value.___AN4)) {}

        static constexpr Quaternion FromComponents(T x, T y, T z, T w) { return {x, y, z, w}; }
        static constexpr Quaternion Identity() { return {T{}, T{}, T{}, T{1}}; }
        constexpr std::array<T, 4> components() const {
            return {this->value.___AN1, this->value.___AN2, this->value.___AN3, this->value.___AN4};
        }
        constexpr bool operator==(const Quaternion&) const = default;
        auto operator<=>(const Quaternion&) const = delete;
        constexpr Quaternion operator+() const { return *this; }
        constexpr auto operator-() const {
            const auto v = Base::operator-();
            return Quaternion<typename decltype(v)::ElementType>(v);
        }
        template<typename U>
        requires concepts::add_as<const T&, const U&>
        constexpr auto operator+(const Quaternion<U>& q) const {
            const auto v = Base::operator+(static_cast<const d4::Vector<U>&>(q));
            return Quaternion<typename decltype(v)::ElementType>(v);
        }
        template<typename U>
        requires concepts::subtract_as<const T&, const U&>
        constexpr auto operator-(const Quaternion<U>& q) const {
            const auto v = Base::operator-(static_cast<const d4::Vector<U>&>(q));
            return Quaternion<typename decltype(v)::ElementType>(v);
        }
        template<typename U>
        requires concepts::multiply_as<const T&, const U&>
        constexpr auto operator*(const Quaternion<U>& q) const;
        template<typename S>
        requires(!concepts::is_vector<S>) && concepts::multiply_as<const T&, const S&>
        constexpr auto operator*(const S& scalar) const {
            const auto v = Base::operator*(scalar);
            return Quaternion<typename decltype(v)::ElementType>(v);
        }
        template<typename S>
        requires(!concepts::is_vector<S>) && concepts::division_as<const T&, const S&> &&
                (!std::integral<decltype(std::declval<const T&>() / std::declval<const S&>())>)
        constexpr auto operator/(const S& scalar) const {
            const auto v = Base::operator/(scalar);
            return Quaternion<typename decltype(v)::ElementType>(v);
        }
        template<typename U>
        requires concepts::add_assign_as<T&, const U&>
        constexpr Quaternion& operator+=(const Quaternion<U>& q) {
            Base::operator+=(static_cast<const d4::Vector<U>&>(q));
            return *this;
        }
        template<typename U>
        requires concepts::subtract_assign_as<T&, const U&>
        constexpr Quaternion& operator-=(const Quaternion<U>& q) {
            Base::operator-=(static_cast<const d4::Vector<U>&>(q));
            return *this;
        }
        constexpr Quaternion& operator*=(const Quaternion& q) {
            *this = (*this * q).template cast<T>();
            return *this;
        }
        template<typename S>
        requires(!concepts::is_vector<S>) && concepts::multiply_assign_as<T&, const S&>
        constexpr Quaternion& operator*=(const S& scalar) {
            Base::operator*=(scalar);
            return *this;
        }
        template<typename S>
        requires(!concepts::is_vector<S>) && (!std::integral<T>) && concepts::division_assign_as<T&, const S&>
        constexpr Quaternion& operator/=(const S& scalar) {
            Base::operator/=(scalar);
            return *this;
        }
        template<typename U>
        requires concepts::cast_as<T, U>
        constexpr Quaternion<U> cast() const { return Quaternion<U>(Base::template cast<U>()); }
        constexpr auto conjugate() const {
            using R = decltype(-this->value.___AN1);
            return Quaternion<R>{-this->value.___AN1, -this->value.___AN2, -this->value.___AN3, this->value.___AN4};
        }
        constexpr auto norm_squared() const { return this->dot(static_cast<const Base&>(*this)); }
        constexpr auto inverse() const
        requires(!std::integral<T>) && concepts::division_as<const T&, const T&>;
        Quaternion& normalize()
        requires std::floating_point<T>
        {
            Base::normalize();
            return *this;
        }
        Quaternion get_normalized() const
        requires std::floating_point<T>
        {
            return Quaternion(Base::get_normalized());
        }
    };

    using QuaternionF = Quaternion<float>;
    using QuaternionD = Quaternion<double>;

    template<typename T>
    Quaternion(const d4::Vector<T>&) -> Quaternion<T>;

    template<typename S, typename T>
    requires(!concepts::is_vector<S>) && concepts::multiply_as<const S&, const T&>
    constexpr auto operator*(const S& scalar, const Quaternion<T>& q);

    // 回転を表す単位UnitQuaternion。成分はglTFと同じx,y,z,w順で取得する。
    template<std::floating_point T = float, HandedCoordinate handed = DEFAULT_COORDINATE_SYSTEM>
    class UnitQuaternion {
        Quaternion<T> _quaternion;

        constexpr explicit UnitQuaternion(std::array<T, 4> components);

    public:
        constexpr UnitQuaternion();

        // 有限で非ゼロの成分を正規化する。入力のスケールには依存しない。
        static UnitQuaternion FromComponents(T x, T y, T z, T w);
        static UnitQuaternion FromQuaternion(const Quaternion<T>& value);
        static UnitQuaternion FromAxisAngle(Radian angle, const Vector_<T, VSL_NAMESPACE::D3>& axis);
        static UnitQuaternion FromAxisAngle(Degree angle, const Vector_<T, VSL_NAMESPACE::D3>& axis);

        constexpr std::array<T, 4> components() const;
        constexpr const Quaternion<T>& quaternion() const { return _quaternion; }
        constexpr bool operator==(const UnitQuaternion&) const = default;
        constexpr Quaternion<T> operator+(const UnitQuaternion& other) const { return _quaternion + other._quaternion; }
        constexpr Quaternion<T> operator-(const UnitQuaternion& other) const { return _quaternion - other._quaternion; }
        template<typename S>
        requires std::is_arithmetic_v<S>
        constexpr auto operator*(const S& scalar) const { return _quaternion * scalar; }
        template<typename S>
        requires std::is_arithmetic_v<S>
        constexpr auto operator/(const S& scalar) const { return _quaternion / scalar; }
        constexpr UnitQuaternion inverse() const;
        // q1*q2はq2→q1の順で回転する。
        UnitQuaternion operator*(const UnitQuaternion& other) const;
        constexpr Vector_<T, VSL_NAMESPACE::D3> rotate(const Vector_<T, VSL_NAMESPACE::D3>& vector) const;
        template<VectorConvention convention = DEFAULT_VECTOR_CONVENTION>
        constexpr Mat4x4<T> to_matrix() const;
    };

    using UnitQuaternionF = UnitQuaternion<float>;
    using UnitQuaternionD = UnitQuaternion<double>;

    template<typename S, std::floating_point T, HandedCoordinate handed>
    requires std::is_arithmetic_v<S>
    constexpr auto operator*(const S& scalar, const UnitQuaternion<T, handed>& q);

    namespace math {
        // 経路の選択はtemplate引数で指定する。有限のamountを受け取り、範囲外は外挿する。
        template<InterpolationPath path = DEFAULT_INTERPOLATION_PATH, std::floating_point T, HandedCoordinate handed>
        UnitQuaternion<T, handed> slerp(const UnitQuaternion<T, handed>& first, const UnitQuaternion<T, handed>& second, T amount);
    } // namespace math

    namespace matrix {
        // 拡縮→回転→平行移動を合成する。ゼロ・負の拡縮も許可する。
        template<VectorConvention convention = DEFAULT_VECTOR_CONVENTION, std::floating_point T, HandedCoordinate handed>
        Mat4x4<T> make_trs(const Vector_<T, VSL_NAMESPACE::D3>& translation, const UnitQuaternion<T, handed>& rotation,
                           const Vector_<T, VSL_NAMESPACE::D3>& scaling);
    } // namespace matrix

    template<typename T>
    template<typename U>
    requires concepts::multiply_as<const T&, const U&>
    constexpr auto Quaternion<T>::operator*(const Quaternion<U>& q) const {
        const auto a = components();
        const auto b = q.components();
        using R = decltype(a[3] * b[0] + a[0] * b[3] + a[1] * b[2] - a[2] * b[1]);
        return Quaternion<R>{a[3] * b[0] + a[0] * b[3] + a[1] * b[2] - a[2] * b[1],
                             a[3] * b[1] - a[0] * b[2] + a[1] * b[3] + a[2] * b[0],
                             a[3] * b[2] + a[0] * b[1] - a[1] * b[0] + a[2] * b[3],
                             a[3] * b[3] - a[0] * b[0] - a[1] * b[1] - a[2] * b[2]};
    }

    template<typename S, typename T>
    requires(!concepts::is_vector<S>) && concepts::multiply_as<const S&, const T&>
    constexpr auto operator*(const S& scalar, const Quaternion<T>& q) {
        const auto v = scalar * static_cast<const d4::Vector<T>&>(q);
        return Quaternion<typename decltype(v)::ElementType>(v);
    }

    template<typename T>
    constexpr auto Quaternion<T>::inverse() const
    requires(!std::integral<T>) && concepts::division_as<const T&, const T&>
    {
        if constexpr (std::floating_point<T>) {
            const auto a = components();
            for (const auto value : a)
                if (!std::isfinite(value))
                    throw std::domain_error("Quaternion inverse requires finite components");
            const auto scale = std::max({std::abs(a[0]), std::abs(a[1]), std::abs(a[2]), std::abs(a[3])});
            if (scale == 0)
                throw std::domain_error("Zero quaternion has no inverse");
            const auto scaled = *this / scale;
            const auto result = (scaled.conjugate() / scaled.norm_squared()) / scale;
            for (const auto value : result.components())
                if (!std::isfinite(value))
                    throw std::overflow_error("Quaternion inverse is not representable");
            return result;
        } else {
            const auto squared = norm_squared();
            if (squared == decltype(squared){})
                throw std::domain_error("Quaternion norm squared is not invertible");
            return conjugate() / squared;
        }
    }

    template<typename S, std::floating_point T, HandedCoordinate handed>
    requires std::is_arithmetic_v<S>
    constexpr auto operator*(const S& scalar, const UnitQuaternion<T, handed>& q) {
        return scalar * q.quaternion();
    }

    template<std::floating_point T, HandedCoordinate handed>
    constexpr UnitQuaternion<T, handed>::UnitQuaternion(std::array<T, 4> components)
        : _quaternion(components[0], components[1], components[2], components[3]) {}

    template<std::floating_point T, HandedCoordinate handed>
    constexpr UnitQuaternion<T, handed>::UnitQuaternion() : _quaternion{0, 0, 0, 1} {}

    template<std::floating_point T, HandedCoordinate handed>
    UnitQuaternion<T, handed> UnitQuaternion<T, handed>::FromComponents(T x, T y, T z, T w) {
        if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z) || !std::isfinite(w))
            throw std::invalid_argument("UnitQuaternion components must be finite");
        const auto scale = std::max({std::abs(x), std::abs(y), std::abs(z), std::abs(w)});
        if (scale == 0)
            throw std::invalid_argument("UnitQuaternion must be nonzero");
        x /= scale;
        y /= scale;
        z /= scale;
        w /= scale;
        const auto length = std::sqrt(x * x + y * y + z * z + w * w);
        return UnitQuaternion(std::array<T, 4>{x / length, y / length, z / length, w / length});
    }

    template<std::floating_point T, HandedCoordinate handed>
    UnitQuaternion<T, handed> UnitQuaternion<T, handed>::FromQuaternion(const Quaternion<T>& value) {
        const auto q = value.components();
        return FromComponents(q[0], q[1], q[2], q[3]);
    }

    template<std::floating_point T, HandedCoordinate handed>
    UnitQuaternion<T, handed> UnitQuaternion<T, handed>::FromAxisAngle(Radian angle, const Vector_<T, VSL_NAMESPACE::D3>& axis) {
        if (!std::isfinite(angle.value))
            throw std::invalid_argument("UnitQuaternion angle must be finite");
        const auto values = math::to_array(axis);
        for (auto value : values)
            if (!std::isfinite(value))
                throw std::invalid_argument("UnitQuaternion axis must be finite");
        const auto scale = std::max({std::abs(values[0]), std::abs(values[1]), std::abs(values[2])});
        if (scale == 0)
            throw std::invalid_argument("UnitQuaternion axis must be nonzero");
        const auto unit = math::to_array(math::normalized(axis / scale));
        T sine = static_cast<T>(std::sin(angle.value / 2));
        if constexpr (handed == HandedCoordinate::LEFT)
            sine = -sine;
        const T cosine = static_cast<T>(std::cos(angle.value / 2));
        return FromComponents(unit[0] * sine, unit[1] * sine, unit[2] * sine, cosine);
    }

    template<std::floating_point T, HandedCoordinate handed>
    UnitQuaternion<T, handed> UnitQuaternion<T, handed>::FromAxisAngle(Degree angle, const Vector_<T, VSL_NAMESPACE::D3>& axis) {
        return FromAxisAngle(static_cast<Radian>(angle), axis);
    }

    template<std::floating_point T, HandedCoordinate handed>
    constexpr std::array<T, 4> UnitQuaternion<T, handed>::components() const {
        return _quaternion.components();
    }

    template<std::floating_point T, HandedCoordinate handed>
    constexpr UnitQuaternion<T, handed> UnitQuaternion<T, handed>::inverse() const {
        const auto q = components();
        return UnitQuaternion(std::array<T, 4>{-q[0], -q[1], -q[2], q[3]});
    }

    template<std::floating_point T, HandedCoordinate handed>
    UnitQuaternion<T, handed> UnitQuaternion<T, handed>::operator*(const UnitQuaternion& other) const {
        return FromQuaternion(_quaternion * other._quaternion);
    }

    template<std::floating_point T, HandedCoordinate handed>
    constexpr Vector_<T, VSL_NAMESPACE::D3> UnitQuaternion<T, handed>::rotate(const Vector_<T, VSL_NAMESPACE::D3>& vector) const {
        const auto q = components();
        const Vector_<T, VSL_NAMESPACE::D3> axis{q[0], q[1], q[2]};
        const auto cross = axis.cross(vector);
        return vector + cross * (T{2} * q[3]) + axis.cross(cross) * T{2};
    }

    template<std::floating_point T, HandedCoordinate handed>
    template<VectorConvention convention>
    constexpr Mat4x4<T> UnitQuaternion<T, handed>::to_matrix() const {
        const auto q = components();
        const T x = q[0], y = q[1], z = q[2], w = q[3];
        const Mat4x4<T> result{{1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w), 0},
                               {2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w), 0},
                               {2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y), 0},
                               {0, 0, 0, 1}};
        return matrix::detail::with_vector_convention<convention>(result);
    }

    namespace math {
        template<InterpolationPath path, std::floating_point T, HandedCoordinate handed>
        UnitQuaternion<T, handed> slerp(const UnitQuaternion<T, handed>& first, const UnitQuaternion<T, handed>& second, T amount) {
            if (!std::isfinite(amount))
                throw std::invalid_argument("UnitQuaternion interpolation amount must be finite");
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
            return UnitQuaternion<T, handed>::FromComponents(result[0], result[1], result[2], result[3]);
        }
    } // namespace math

    namespace matrix {
        template<VectorConvention convention, std::floating_point T, HandedCoordinate handed>
        Mat4x4<T> make_trs(const Vector_<T, VSL_NAMESPACE::D3>& translation, const UnitQuaternion<T, handed>& rotation,
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
