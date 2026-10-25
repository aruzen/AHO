#pragma once
#include "../define.hpp"
#include "../concepts.hpp"

#include "VSL/dimension.hpp"

#include <compare>
#include <algorithm>
#include <cmath>
#include <type_traits>
#include <stdexcept>
#include "ov.hpp"
#include "math/coordinate.hpp"
#include "math/mat.hpp"

namespace AHO_NAMESPACE {
    template<typename R, VSL_NAMESPACE::is_dimension D, typename CoordinateInfo>
    struct Vector_ {
    };

    template<typename R = int, typename... Args>
    requires concepts::sames_as<R, Args...> && (!concepts::IS_COORDINATE_SET<R>)
    struct Vector : public Vector_<R,
                                   typename VSL_NAMESPACE::Dimension<sizeof...(Args) + 1>::Value,
                                   typename AHO_NAMESPACE::coordinate::DefaultCoordinateInfo<
                                       typename VSL_NAMESPACE::Dimension<sizeof...(Args) + 1>::Value>::Value> {
        constexpr Vector() {};

        constexpr Vector(R x, Args... args) : Vector_<R,
                                                      typename VSL_NAMESPACE::Dimension<sizeof...(Args) + 1>::Value,
                                                      typename AHO_NAMESPACE::coordinate::DefaultCoordinateInfo<
                                                          typename VSL_NAMESPACE::Dimension<sizeof...(Args) + 1>::Value>::Value>(x, args...) {};
    };

    namespace concepts {
        template<typename T>
        concept is_vector = requires {
            typename std::remove_cvref_t<T>::Dimension;
            typename std::remove_cvref_t<T>::ElementType;
            typename std::remove_cvref_t<T>::CoordinateInfo;
            requires std::convertible_to<std::remove_cvref_t<T>,
                                         Vector_<typename std::remove_cvref_t<T>::ElementType,
                                                 typename std::remove_cvref_t<T>::Dimension,
                                                 typename std::remove_cvref_t<T>::CoordinateInfo>>;
        };
    } // namespace concepts

    namespace d1 {
        template<typename R, typename CI = typename AHO_NAMESPACE::coordinate::DefaultCoordinateInfo<VSL_NAMESPACE::D1>::Value>
        struct Vector : public AHO_NAMESPACE::Vector_<R, VSL_NAMESPACE::D1, CI> {
            constexpr Vector() {};

            constexpr Vector(R v) : AHO_NAMESPACE::Vector_<R, VSL_NAMESPACE::D1, CI>(v) {};
        };
        using VectorI = AHO_NAMESPACE::Vector_<int, VSL_NAMESPACE::D1>;
        using VectorF = AHO_NAMESPACE::Vector_<float, VSL_NAMESPACE::D1>;
        using VectorD = AHO_NAMESPACE::Vector_<double, VSL_NAMESPACE::D1>;
    } // namespace d1

    namespace d2 {
        template<typename R, typename CI = typename AHO_NAMESPACE::coordinate::DefaultCoordinateInfo<VSL_NAMESPACE::D2>::Value>
        struct Vector : public AHO_NAMESPACE::Vector_<R, VSL_NAMESPACE::D2, CI> {
            constexpr Vector() {};

            constexpr Vector(R v1, R v2) : AHO_NAMESPACE::Vector_<R, VSL_NAMESPACE::D2, CI>(v1, v2) {};
        };
        using VectorI = AHO_NAMESPACE::Vector_<int, VSL_NAMESPACE::D2>;
        using VectorF = AHO_NAMESPACE::Vector_<float, VSL_NAMESPACE::D2>;
        using VectorD = AHO_NAMESPACE::Vector_<double, VSL_NAMESPACE::D2>;
    } // namespace d2

    namespace d3 {
        template<typename R, typename CI = typename AHO_NAMESPACE::coordinate::DefaultCoordinateInfo<VSL_NAMESPACE::D3>::Value>
        struct Vector : public AHO_NAMESPACE::Vector_<R, VSL_NAMESPACE::D3, CI> {
            constexpr Vector() {};

            constexpr Vector(R x, R y, R z) : AHO_NAMESPACE::Vector_<R, VSL_NAMESPACE::D3, CI>(x, y, z) {};
        };
        using VectorI = AHO_NAMESPACE::Vector_<int, VSL_NAMESPACE::D3>;
        using VectorF = AHO_NAMESPACE::Vector_<float, VSL_NAMESPACE::D3>;
        using VectorD = AHO_NAMESPACE::Vector_<double, VSL_NAMESPACE::D3>;
    } // namespace d3

    template<typename R, typename CI>
    struct Vector_<R, VSL_NAMESPACE::D1, CI> {
        using Dimension = typename VSL_NAMESPACE::D1;
        using ElementType = R;
        using CoordinateInfo = CI;

        coordinate::CoordinateSet<R, CI> value;

        constexpr Vector_() {};

        constexpr Vector_(R value) : value(value) {};

        constexpr Vector_(const coordinate::CoordinateSet<R, CI>& value) : value(value) {};

        constexpr bool operator<(const Vector_<R, VSL_NAMESPACE::D1, CI>& p) const;

        constexpr auto operator<=>(const Vector_<R, VSL_NAMESPACE::D1, CI>&) const = default;

        constexpr Vector_<R, VSL_NAMESPACE::D1, CI> operator+() const;

        constexpr Vector_<R, VSL_NAMESPACE::D1, CI> operator-() const;

        template<typename T>
        requires concepts::add_as<const R&, const T&>
        constexpr auto operator+(const Vector_<T, VSL_NAMESPACE::D1, CI>& p) const;

        template<typename T>
        requires concepts::subtract_as<const R&, const T&>
        constexpr auto operator-(const Vector_<T, VSL_NAMESPACE::D1, CI>& p) const;

        template<typename T>
        requires(!concepts::is_vector<T>) && concepts::multiply_as<const R&, const T&>
        constexpr auto operator*(const T& m) const;

        template<typename T>
        requires concepts::division_as<const R&, const T&>
        constexpr auto operator/(const T& m) const;

        template<typename T>
        requires concepts::add_assign_as<R&, const T&>
        constexpr Vector_<R, VSL_NAMESPACE::D1, CI>& operator+=(const Vector_<T, VSL_NAMESPACE::D1, CI>& p);

        template<typename T>
        requires concepts::subtract_assign_as<R&, const T&>
        constexpr Vector_<R, VSL_NAMESPACE::D1, CI>& operator-=(const Vector_<T, VSL_NAMESPACE::D1, CI>& p);

        template<typename T>
        requires concepts::multiply_assign_as<R&, const T&>
        constexpr Vector_<R, VSL_NAMESPACE::D1, CI>& operator*=(const T& m);

        template<typename T>
        requires concepts::division_assign_as<R&, const T&>
        constexpr Vector_<R, VSL_NAMESPACE::D1, CI>& operator/=(const T& m);

        template<typename T>
        requires concepts::cast_as<R, T>
        constexpr Vector_<T, VSL_NAMESPACE::D1, CI> cast() const;

        template<typename T>
        requires concepts::is_vector<T> &&
                 std::convertible_to<T, Vector_<typename T::ElementType, VSL_NAMESPACE::D1, CI>>
        constexpr auto dot(const T& m) const;

        template<typename T>
        requires concepts::is_vector<T> && std::convertible_to<Vector_<typename T::ElementType, VSL_NAMESPACE::D1>, T>
        [[deprecated("cross() is undefined in 1D. Consider checking vector dimension.")]]
        constexpr auto cross(const T& m) const;

        template<typename T>
        requires concepts::is_vector<T> &&
                 std::convertible_to<T, Vector_<typename T::ElementType, VSL_NAMESPACE::D1, CI>>
        constexpr auto hadamard(const T& m) const;

        constexpr auto length() const;

        constexpr Vector_<R, VSL_NAMESPACE::D1, CI>& normalize();

        constexpr auto get_normalized() const;

        constexpr operator Mat<R, 1, 1>() const;
    };

    template<typename R, typename CI>
    struct Vector_<R, VSL_NAMESPACE::D2, CI> {
        using Dimension = typename VSL_NAMESPACE::D2;
        using ElementType = R;
        using CoordinateInfo = CI;

        coordinate::CoordinateSet<R, CI> value;

        constexpr Vector_() {};

        constexpr Vector_(R v1, R v2) : value(v1, v2) {};

        constexpr Vector_(const coordinate::CoordinateSet<R, CI>& value) : value(value) {};

        constexpr bool operator<(const Vector_<R, VSL_NAMESPACE::D2, CI>& p) const;

        constexpr auto operator<=>(const Vector_<R, VSL_NAMESPACE::D2, CI>&) const = default;

        constexpr Vector_<R, VSL_NAMESPACE::D2, CI> operator+() const;

        constexpr Vector_<R, VSL_NAMESPACE::D2, CI> operator-() const;

        template<typename T>
        requires concepts::add_as<const R&, const T&>
        constexpr auto operator+(const Vector_<T, VSL_NAMESPACE::D2, CI>& p) const;

        template<typename T>
        requires concepts::subtract_as<const R&, const T&>
        constexpr auto operator-(const Vector_<T, VSL_NAMESPACE::D2, CI>& p) const;

        template<typename T>
        requires(!concepts::is_vector<T>) && concepts::multiply_as<const R&, const T&>
        constexpr auto operator*(const T& m) const;

        template<typename T>
        requires concepts::division_as<const R&, const T&>
        constexpr auto operator/(const T& m) const;

        template<typename T>
        requires concepts::add_assign_as<R&, const T&>
        constexpr Vector_<R, VSL_NAMESPACE::D2, CI>& operator+=(const Vector_<T, VSL_NAMESPACE::D2, CI>& p);

        template<typename T>
        requires concepts::subtract_assign_as<R&, const T&>
        constexpr Vector_<R, VSL_NAMESPACE::D2, CI>& operator-=(const Vector_<T, VSL_NAMESPACE::D2, CI>& p);

        template<typename T>
        requires concepts::multiply_assign_as<R&, const T&>
        constexpr Vector_<R, VSL_NAMESPACE::D2, CI>& operator*=(const T& m);

        template<typename T>
        requires concepts::division_assign_as<R&, const T&>
        constexpr Vector_<R, VSL_NAMESPACE::D2, CI>& operator/=(const T& m);

        template<typename T>
        requires concepts::cast_as<R, T>
        constexpr Vector_<T, VSL_NAMESPACE::D2, CI> cast() const;

        template<typename T>
        requires concepts::is_vector<T> && std::convertible_to<T, Vector_<typename T::ElementType, VSL_NAMESPACE::D2, CI>>
        constexpr auto dot(const T& m) const;

        template<typename T>
        requires concepts::is_vector<T> && std::convertible_to<T, Vector_<typename T::ElementType, VSL_NAMESPACE::D2, CI>>
        constexpr auto cross(const T& m) const;

        template<typename T>
        requires concepts::is_vector<T> && std::convertible_to<T, Vector_<typename T::ElementType, VSL_NAMESPACE::D2, CI>>
        constexpr auto hadamard(const T& m) const;

        constexpr auto length() const;

        constexpr Vector_<R, VSL_NAMESPACE::D2, CI>& normalize();

        constexpr auto get_normalized() const;

        constexpr operator Mat<R, 1, 2>() const;

        constexpr operator Mat<R, 2, 2>() const;
    };

    template<typename R, typename CI>
    struct Vector_<R, VSL_NAMESPACE::D3, CI> {
        using Dimension = typename VSL_NAMESPACE::D3;
        using ElementType = R;
        using CoordinateInfo = CI;

        coordinate::CoordinateSet<R, CI> value;

        constexpr Vector_() {};

        constexpr Vector_(R x, R y, R z) : value(x, y, z) {};

        constexpr Vector_(const coordinate::CoordinateSet<R, CI>& value) : value(value) {};

        constexpr bool operator<(const Vector_<R, VSL_NAMESPACE::D3, CI>& p) const;

        constexpr auto operator<=>(const Vector_<R, VSL_NAMESPACE::D3, CI>&) const = default;

        constexpr Vector_<R, VSL_NAMESPACE::D3, CI> operator+() const;

        constexpr Vector_<R, VSL_NAMESPACE::D3, CI> operator-() const;

        template<typename T>
        requires concepts::add_as<const R&, const T&>
        constexpr auto operator+(const Vector_<T, VSL_NAMESPACE::D3, CI>& p) const;

        template<typename T>
        requires concepts::subtract_as<const R&, const T&>
        constexpr auto operator-(const Vector_<T, VSL_NAMESPACE::D3, CI>& p) const;

        template<typename T>
        requires(!concepts::is_vector<T>) && concepts::multiply_as<const R&, const T&>
        constexpr auto operator*(const T& m) const;

        template<typename T>
        requires concepts::division_as<const R&, const T&>
        constexpr auto operator/(const T& m) const;

        template<typename T>
        requires concepts::add_assign_as<R&, const T&>
        constexpr Vector_<R, VSL_NAMESPACE::D3, CI>& operator+=(const Vector_<T, VSL_NAMESPACE::D3, CI>& p);

        template<typename T>
        requires concepts::subtract_assign_as<R&, const T&>
        constexpr Vector_<R, VSL_NAMESPACE::D3, CI>& operator-=(const Vector_<T, VSL_NAMESPACE::D3, CI>& p);

        template<typename T>
        requires concepts::multiply_assign_as<R&, const T&>
        constexpr Vector_<R, VSL_NAMESPACE::D3, CI>& operator*=(const T& m);

        template<typename T>
        requires concepts::division_assign_as<R&, const T&>
        constexpr Vector_<R, VSL_NAMESPACE::D3, CI>& operator/=(const T& m);

        template<typename T>
        requires concepts::cast_as<R, T>
        constexpr Vector_<T, VSL_NAMESPACE::D3, CI> cast() const;

        template<typename T>
        requires concepts::is_vector<T> && std::convertible_to<T, Vector_<typename T::ElementType, VSL_NAMESPACE::D3, CI>>
        constexpr auto dot(const T& m) const;

        template<typename T>
        requires concepts::is_vector<T> && std::convertible_to<T, Vector_<typename T::ElementType, VSL_NAMESPACE::D3, CI>>
        constexpr auto cross(const T& m) const;

        template<typename T>
        requires concepts::is_vector<T> && std::convertible_to<T, Vector_<typename T::ElementType, VSL_NAMESPACE::D3, CI>>
        constexpr auto hadamard(const T& m) const;

        constexpr auto length() const;

        constexpr Vector_<R, VSL_NAMESPACE::D3, CI>& normalize();

        constexpr auto get_normalized() const;

        constexpr operator Mat<R, 1, 4>() const;

        constexpr operator Mat<R, 4, 4>() const;
    };

    template<typename R, typename CI>
    struct Vector_<R, VSL_NAMESPACE::D4, CI> {
        using Dimension = VSL_NAMESPACE::D4;
        using ElementType = R;
        using CoordinateInfo = CI;
        coordinate::CoordinateSet<R, CI> value{};

        constexpr Vector_() = default;
        constexpr Vector_(R x, R y, R z, R w) : value(x, y, z, w) {}
        constexpr Vector_(const coordinate::CoordinateSet<R, CI>& value) : value(value) {}
        constexpr bool operator==(const Vector_&) const = default;
        constexpr auto operator<=>(const Vector_&) const = default;
        constexpr Vector_ operator+() const { return *this; }
        constexpr auto operator-() const {
            return Vector_<decltype(-value.___AN1), Dimension, CI>{
                -value.___AN1, -value.___AN2, -value.___AN3, -value.___AN4};
        }
        template<typename T>
        requires concepts::add_as<const R&, const T&>
        constexpr auto operator+(const Vector_<T, Dimension, CI>& p) const {
            return Vector_<decltype(value.___AN1 + p.value.___AN1), Dimension, CI>{
                value.___AN1 + p.value.___AN1, value.___AN2 + p.value.___AN2,
                value.___AN3 + p.value.___AN3, value.___AN4 + p.value.___AN4};
        }
        template<typename T>
        requires concepts::subtract_as<const R&, const T&>
        constexpr auto operator-(const Vector_<T, Dimension, CI>& p) const {
            return Vector_<decltype(value.___AN1 - p.value.___AN1), Dimension, CI>{
                value.___AN1 - p.value.___AN1, value.___AN2 - p.value.___AN2,
                value.___AN3 - p.value.___AN3, value.___AN4 - p.value.___AN4};
        }
        template<typename T>
        requires(!concepts::is_vector<T>) && concepts::multiply_as<const R&, const T&>
        constexpr auto operator*(const T& m) const {
            return Vector_<decltype(value.___AN1 * m), Dimension, CI>{
                value.___AN1 * m, value.___AN2 * m, value.___AN3 * m, value.___AN4 * m};
        }
        template<typename T>
        requires(!concepts::is_vector<T>) && concepts::division_as<const R&, const T&>
        constexpr auto operator/(const T& m) const {
            return Vector_<decltype(value.___AN1 / m), Dimension, CI>{
                value.___AN1 / m, value.___AN2 / m, value.___AN3 / m, value.___AN4 / m};
        }
        template<typename T>
        requires concepts::add_assign_as<R&, const T&>
        constexpr Vector_& operator+=(const Vector_<T, Dimension, CI>& p);
        template<typename T>
        requires concepts::subtract_assign_as<R&, const T&>
        constexpr Vector_& operator-=(const Vector_<T, Dimension, CI>& p);
        template<typename T>
        requires(!concepts::is_vector<T>) && concepts::multiply_assign_as<R&, const T&>
        constexpr Vector_& operator*=(const T& m);
        template<typename T>
        requires(!concepts::is_vector<T>) && concepts::division_assign_as<R&, const T&>
        constexpr Vector_& operator/=(const T& m);
        template<typename T>
        requires concepts::cast_as<R, T>
        constexpr Vector_<T, Dimension, CI> cast() const {
            return {static_cast<T>(value.___AN1), static_cast<T>(value.___AN2),
                    static_cast<T>(value.___AN3), static_cast<T>(value.___AN4)};
        }
        template<typename T>
        requires concepts::multiply_as<const R&, const T&>
        constexpr auto dot(const Vector_<T, Dimension, CI>& p) const {
            return value.___AN1 * p.value.___AN1 + value.___AN2 * p.value.___AN2 +
                   value.___AN3 * p.value.___AN3 + value.___AN4 * p.value.___AN4;
        }
        template<typename T>
        requires concepts::multiply_as<const R&, const T&>
        constexpr auto hadamard(const Vector_<T, Dimension, CI>& p) const {
            return Vector_<decltype(value.___AN1 * p.value.___AN1), Dimension, CI>{
                value.___AN1 * p.value.___AN1, value.___AN2 * p.value.___AN2,
                value.___AN3 * p.value.___AN3, value.___AN4 * p.value.___AN4};
        }
        auto length() const
        requires std::is_arithmetic_v<R>
        {
            using Result = std::conditional_t<std::floating_point<R>, R, long double>;
            return std::hypot(std::hypot(static_cast<Result>(value.___AN1), static_cast<Result>(value.___AN2)),
                              std::hypot(static_cast<Result>(value.___AN3), static_cast<Result>(value.___AN4)));
        }
        Vector_& normalize()
        requires std::floating_point<R>;
        Vector_ get_normalized() const
        requires std::floating_point<R>
        {
            auto result = *this;
            result.normalize();
            return result;
        }
        constexpr operator Mat<R, 1, 4>() const {
            return {{value.___AN1, value.___AN2, value.___AN3, value.___AN4}};
        }
    };

    namespace d4 {
        template<typename R, typename CI = typename coordinate::DefaultCoordinateInfo<VSL_NAMESPACE::D4>::Value>
        using Vector = Vector_<R, VSL_NAMESPACE::D4, CI>;
        using VectorI = Vector<int>;
        using VectorF = Vector<float>;
        using VectorD = Vector<double>;
    } // namespace d4

    namespace product::dot {
        template<typename Dim, typename CI, typename R1, typename R2>
        constexpr auto operator*(Vector_<R1, Dim, CI> v1, Vector_<R2, Dim, CI> v2);
    }

    namespace product::cross {
        template<typename Dim, typename CI, typename R1, typename R2>
        constexpr auto operator*(Vector_<R1, Dim, CI> v1, Vector_<R2, Dim, CI> v2);
    }

    namespace product::hadamard {
        template<typename Dim, typename CI, typename R1, typename R2>
        constexpr auto operator*(Vector_<R1, Dim, CI> v1, Vector_<R2, Dim, CI> v2);
    }

    template<typename R, typename CI>
    Vector_(coordinate::CoordinateSet<R, CI>)
        -> Vector_<R, typename CI::Dimension, CI>;

    // ---------------------------------------------------------------------------------------------------------------------

    template<typename L, typename R, typename Dim, typename CI>
    requires(!concepts::is_vector<L>) && concepts::multiply_as<const L&, const R&>
    constexpr auto operator*(const L& l, Vector_<R, Dim, CI> v1);

    // ---------------------------------------------------------------------------------------------------------------------

    template<typename R, typename CI>
    constexpr bool Vector_<R, VSL_NAMESPACE::D1, CI>::operator<(const Vector_<R, VSL_NAMESPACE::D1, CI>& p) const {
        return length() < p.length();
    }

    template<typename R, typename CI>
    constexpr Vector_<R, VSL_NAMESPACE::D1, CI> Vector_<R, VSL_NAMESPACE::D1, CI>::operator+() const {
        return *this;
    }

    template<typename R, typename CI>
    constexpr Vector_<R, VSL_NAMESPACE::D1, CI> Vector_<R, VSL_NAMESPACE::D1, CI>::operator-() const {
        return Vector_<R, VSL_NAMESPACE::D1, CI>(-value.___AN1);
    }

    template<typename R, typename CI>
    template<typename T>
    requires concepts::add_as<const R&, const T&>
    constexpr auto Vector_<R, VSL_NAMESPACE::D1, CI>::operator+(const Vector_<T, VSL_NAMESPACE::D1, CI>& p) const {
        return Vector_<decltype(value.___AN1 + p.value.___AN1), VSL_NAMESPACE::D1, CI>(value.___AN1 + p.value.___AN1);
    }

    template<typename R, typename CI>
    template<typename T>
    requires concepts::subtract_as<const R&, const T&>
    constexpr auto Vector_<R, VSL_NAMESPACE::D1, CI>::operator-(const Vector_<T, VSL_NAMESPACE::D1, CI>& p) const {
        return Vector_<decltype(value.___AN1 - p.value.___AN1), VSL_NAMESPACE::D1, CI>(value.___AN1 - p.value.___AN1);
    }

    template<typename R, typename CI>
    template<typename T>
    requires(!concepts::is_vector<T>) && concepts::multiply_as<const R&, const T&>
    constexpr auto Vector_<R, VSL_NAMESPACE::D1, CI>::operator*(const T& m) const {
        return Vector_<decltype(value.___AN1 * m), VSL_NAMESPACE::D1, CI>(value.___AN1 * m);
    }

    template<typename R, typename CI>
    template<typename T>
    requires concepts::division_as<const R&, const T&>
    constexpr auto Vector_<R, VSL_NAMESPACE::D1, CI>::operator/(const T& m) const {
        return Vector_<decltype(value.___AN1 / m), VSL_NAMESPACE::D1, CI>(value.___AN1 / m);
    }

    template<typename R, typename CI>
    template<typename T>
    requires concepts::add_assign_as<R&, const T&>
    constexpr Vector_<R, VSL_NAMESPACE::D1, CI>& Vector_<R, VSL_NAMESPACE::D1, CI>::operator+=(const Vector_<T, VSL_NAMESPACE::D1, CI>& p) {
        this->value.___AN1 += p.value.___AN1;
        return *this;
    }

    template<typename R, typename CI>
    template<typename T>
    requires concepts::subtract_assign_as<R&, const T&>
    constexpr Vector_<R, VSL_NAMESPACE::D1, CI>& Vector_<R, VSL_NAMESPACE::D1, CI>::operator-=(const Vector_<T, VSL_NAMESPACE::D1, CI>& p) {
        this->value.___AN1 -= p.value.___AN1;
        return *this;
    }

    template<typename R, typename CI>
    template<typename T>
    requires concepts::multiply_assign_as<R&, const T&>
    constexpr Vector_<R, VSL_NAMESPACE::D1, CI>& Vector_<R, VSL_NAMESPACE::D1, CI>::operator*=(const T& p) {
        this->value.___AN1 *= p;
        return *this;
    }

    template<typename R, typename CI>
    template<typename T>
    requires concepts::division_assign_as<R&, const T&>
    constexpr Vector_<R, VSL_NAMESPACE::D1, CI>& Vector_<R, VSL_NAMESPACE::D1, CI>::operator/=(const T& p) {
        this->value.___AN1 /= p;
        return *this;
    }

    template<typename R, typename CI>
    template<typename T>
    requires concepts::cast_as<R, T>
    constexpr Vector_<T, VSL_NAMESPACE::D1, CI> Vector_<R, VSL_NAMESPACE::D1, CI>::cast() const {
        return Vector_<T, VSL_NAMESPACE::D1, CI>((T)value.___AN1);
    }

    template<typename R, typename CI>
    template<typename T>
    requires concepts::is_vector<T> &&
             std::convertible_to<T, Vector_<typename T::ElementType, VSL_NAMESPACE::D1, CI>>
    inline constexpr auto Vector_<R, VSL_NAMESPACE::D1, CI>::dot(const T& m) const {
        return value.___AN1 * m.value.___AN1;
    }

    template<typename R, typename CI>
    template<typename T>
    requires concepts::is_vector<T> && std::convertible_to<Vector_<typename T::ElementType, VSL_NAMESPACE::D1>, T>
    inline constexpr auto Vector_<R, VSL_NAMESPACE::D1, CI>::cross(const T& m) const {
        return (decltype(std::declval<R>() * std::declval<T>()))0;
    }

    template<typename R, typename CI>
    template<typename T>
    requires concepts::is_vector<T> &&
             std::convertible_to<T, Vector_<typename T::ElementType, VSL_NAMESPACE::D1, CI>>
    inline constexpr auto Vector_<R, VSL_NAMESPACE::D1, CI>::hadamard(const T& m) const {
        return Vector_<decltype(value.___AN1 * m.value.___AN1), VSL_NAMESPACE::D1, CI>(value.___AN1 * m.value.___AN1);
    }

    template<typename R, typename CI>
    constexpr auto Vector_<R, VSL_NAMESPACE::D1, CI>::length() const {
        const R component = [&]() {
            if constexpr (CI::ENABLED_X)
                return value.x.value;
            else if constexpr (CI::ENABLED_Y)
                return value.y.value;
            else
                return value.z.value;
        }();
        if constexpr (std::is_integral_v<R> && !std::is_same_v<R, bool>) {
            using Magnitude = std::make_unsigned_t<R>;
            const auto magnitude = static_cast<Magnitude>(component);
            return component < 0 ? static_cast<Magnitude>(Magnitude{} - magnitude) : magnitude;
        } else {
            return std::abs(component);
        }
    }

    template<typename R, typename CI>
    constexpr Vector_<R, VSL_NAMESPACE::D1, CI>& Vector_<R, VSL_NAMESPACE::D1, CI>::normalize() {
        auto& component = [&]() -> R& {
            if constexpr (CI::ENABLED_X)
                return value.x.value;
            else if constexpr (CI::ENABLED_Y)
                return value.y.value;
            else
                return value.z.value;
        }();
        component = component < R{} ? R(-1) : R(1);
        return *this;
    }

    template<typename R, typename CI>
    constexpr auto Vector_<R, VSL_NAMESPACE::D1, CI>::get_normalized() const {
        auto result = *this;
        result.normalize();
        return result;
    }

    template<typename R, typename CI>
    constexpr Vector_<R, VSL_NAMESPACE::D1, CI>::operator Mat<R, 1, 1>() const {
        return Mat<R, 1, 1>({{value.___AN1}});
    }

    // ---------------------------------------------------------------------------------------------------------------------

    template<typename R, typename CI>
    constexpr bool Vector_<R, VSL_NAMESPACE::D2, CI>::operator<(const Vector_<R, VSL_NAMESPACE::D2, CI>& p) const {
        return length() < p.length();
    }

    template<typename R, typename CI>
    constexpr Vector_<R, VSL_NAMESPACE::D2, CI> Vector_<R, VSL_NAMESPACE::D2, CI>::operator+() const {
        return *this;
    }

    template<typename R, typename CI>
    constexpr Vector_<R, VSL_NAMESPACE::D2, CI> Vector_<R, VSL_NAMESPACE::D2, CI>::operator-() const {
        return Vector_<R, VSL_NAMESPACE::D2, CI>(-value.___AN1, -value.___AN2);
    }

    template<typename R, typename CI>
    template<typename T>
    requires concepts::add_as<const R&, const T&>
    constexpr auto Vector_<R, VSL_NAMESPACE::D2, CI>::operator+(const Vector_<T, VSL_NAMESPACE::D2, CI>& p) const {
        return Vector_<decltype(value.___AN1 + p.value.___AN1), VSL_NAMESPACE::D2, CI>(value.___AN1 + p.value.___AN1, value.___AN2 + p.value.___AN2);
    }

    template<typename R, typename CI>
    template<typename T>
    requires concepts::subtract_as<const R&, const T&>
    constexpr auto Vector_<R, VSL_NAMESPACE::D2, CI>::operator-(const Vector_<T, VSL_NAMESPACE::D2, CI>& p) const {
        return Vector_<decltype(value.___AN1 - p.value.___AN1), VSL_NAMESPACE::D2, CI>(value.___AN1 - p.value.___AN1, value.___AN2 - p.value.___AN2);
    }

    template<typename R, typename CI>
    template<typename T>
    requires(!concepts::is_vector<T>) && concepts::multiply_as<const R&, const T&>
    constexpr auto Vector_<R, VSL_NAMESPACE::D2, CI>::operator*(const T& m) const {
        return Vector_<decltype(value.___AN1 * m), VSL_NAMESPACE::D2, CI>(value.___AN1 * m, value.___AN2 * m);
    }

    template<typename R, typename CI>
    template<typename T>
    requires concepts::division_as<const R&, const T&>
    constexpr auto Vector_<R, VSL_NAMESPACE::D2, CI>::operator/(const T& m) const {
        return Vector_<decltype(value.___AN1 / m), VSL_NAMESPACE::D2, CI>(value.___AN1 / m, value.___AN2 / m);
    }

    template<typename R, typename CI>
    template<typename T>
    requires concepts::add_assign_as<R&, const T&>
    constexpr Vector_<R, VSL_NAMESPACE::D2, CI>& Vector_<R, VSL_NAMESPACE::D2, CI>::operator+=(const Vector_<T, VSL_NAMESPACE::D2, CI>& p) {
        this->value.___AN1 += p.value.___AN1;
        this->value.___AN2 += p.value.___AN2;
        return *this;
    }

    template<typename R, typename CI>
    template<typename T>
    requires concepts::subtract_assign_as<R&, const T&>
    constexpr Vector_<R, VSL_NAMESPACE::D2, CI>& Vector_<R, VSL_NAMESPACE::D2, CI>::operator-=(const Vector_<T, VSL_NAMESPACE::D2, CI>& p) {
        this->value.___AN1 -= p.value.___AN1;
        this->value.___AN2 -= p.value.___AN2;
        return *this;
    }

    template<typename R, typename CI>
    template<typename T>
    requires concepts::multiply_assign_as<R&, const T&>
    constexpr Vector_<R, VSL_NAMESPACE::D2, CI>& Vector_<R, VSL_NAMESPACE::D2, CI>::operator*=(const T& p) {
        this->value.___AN1 *= p;
        this->value.___AN2 *= p;
        return *this;
    }

    template<typename R, typename CI>
    template<typename T>
    requires concepts::division_assign_as<R&, const T&>
    constexpr Vector_<R, VSL_NAMESPACE::D2, CI>& Vector_<R, VSL_NAMESPACE::D2, CI>::operator/=(const T& p) {
        this->value.___AN1 /= p;
        this->value.___AN2 /= p;
        return *this;
    }

    template<typename R, typename CI>
    template<typename T>
    requires concepts::cast_as<R, T>
    constexpr Vector_<T, VSL_NAMESPACE::D2, CI> Vector_<R, VSL_NAMESPACE::D2, CI>::cast() const {
        return Vector_<T, VSL_NAMESPACE::D2, CI>((T)value.___AN1, (T)value.___AN2);
    }

    template<typename R, typename CI>
    template<typename T>
    requires concepts::is_vector<T> && std::convertible_to<T, Vector_<typename T::ElementType, VSL_NAMESPACE::D2, CI>>
    constexpr auto Vector_<R, VSL_NAMESPACE::D2, CI>::dot(const T& m) const {
        return value.___AN1 * m.value.___AN1 + value.___AN2 * m.value.___AN2;
    }

    template<typename R, typename CI>
    template<typename T>
    requires concepts::is_vector<T> && std::convertible_to<T, Vector_<typename T::ElementType, VSL_NAMESPACE::D2, CI>>
    constexpr auto Vector_<R, VSL_NAMESPACE::D2, CI>::cross(const T& m) const {
        auto determinant = value.___AN1 * m.value.___AN2 - value.___AN2 * m.value.___AN1;
        // In a right-handed system, X cross Z points along negative Y.
        if constexpr (CI::ENABLED_X && CI::ENABLED_Z)
            determinant = -determinant;
        return Vector_<decltype(determinant), VSL_NAMESPACE::D1,
                       typename coordinate::MakeCoordinateInfo<(!CI::ENABLED_X), (!CI::ENABLED_Y), (!CI::ENABLED_Z)>::Value>(determinant);
    }

    template<typename R, typename CI>
    template<typename T>
    requires concepts::is_vector<T> && std::convertible_to<T, Vector_<typename T::ElementType, VSL_NAMESPACE::D2, CI>>
    constexpr auto Vector_<R, VSL_NAMESPACE::D2, CI>::hadamard(const T& m) const {
        Vector_<decltype(value.___AN1 * m.value.___AN1), VSL_NAMESPACE::D2, CI> result;
        result.value.___AN1 = value.___AN1 * m.value.___AN1;
        result.value.___AN2 = value.___AN2 * m.value.___AN2;
        return result;
    }

    template<typename R, typename CI>
    constexpr auto Vector_<R, VSL_NAMESPACE::D2, CI>::length() const {
        return std::hypot(value.___AN1, value.___AN2);
    }

    template<typename R, typename CI>
    constexpr Vector_<R, VSL_NAMESPACE::D2, CI>& Vector_<R, VSL_NAMESPACE::D2, CI>::normalize() {
        if constexpr (std::floating_point<R>)
            *this = get_normalized();
        else
            *this /= length();
        return *this;
    }

    template<typename R, typename CI>
    constexpr auto Vector_<R, VSL_NAMESPACE::D2, CI>::get_normalized() const {
        if constexpr (std::floating_point<R>) {
            const auto scale = std::max(std::abs(value.___AN1), std::abs(value.___AN2));
            if (scale > 0 && std::isfinite(scale)) {
                const auto scaled = *this / scale;
                return scaled / scaled.length();
            }
        }
        return *this / length();
    }

    template<typename R, typename CI>
    constexpr Vector_<R, VSL_NAMESPACE::D2, CI>::operator Mat<R, 1, 2>() const {
        return Mat<R, 1, 2>({{value.___AN1, value.___AN2}});
    }

    template<typename R, typename CI>
    constexpr Vector_<R, VSL_NAMESPACE::D2, CI>::operator Mat<R, 2, 2>() const {
        return Mat<R, 2, 2>({{value.___AN1, 0}, {0, value.___AN2}});
    }

    // ---------------------------------------------------------------------------------------------------------------------

    template<typename R, typename CI>
    constexpr bool Vector_<R, VSL_NAMESPACE::D3, CI>::operator<(const Vector_<R, VSL_NAMESPACE::D3, CI>& p) const {
        return length() < p.length();
    }

    template<typename R, typename CI>
    constexpr Vector_<R, VSL_NAMESPACE::D3, CI> Vector_<R, VSL_NAMESPACE::D3, CI>::operator+() const {
        return *this;
    }

    template<typename R, typename CI>
    constexpr Vector_<R, VSL_NAMESPACE::D3, CI> Vector_<R, VSL_NAMESPACE::D3, CI>::operator-() const {
        return Vector_<R, VSL_NAMESPACE::D3, CI>(-value.___AN1, -value.___AN2, -value.___AN3);
    }

    template<typename R, typename CI>
    template<typename T>
    requires concepts::add_as<const R&, const T&>
    constexpr auto Vector_<R, VSL_NAMESPACE::D3, CI>::operator+(const Vector_<T, VSL_NAMESPACE::D3, CI>& p) const {
        return Vector_<decltype(value.___AN1 + p.value.___AN1), VSL_NAMESPACE::D3, CI>(value.___AN1 + p.value.___AN1, value.___AN2 + p.value.___AN2, value.___AN3 + p.value.___AN3);
    }

    template<typename R, typename CI>
    template<typename T>
    requires concepts::subtract_as<const R&, const T&>
    constexpr auto Vector_<R, VSL_NAMESPACE::D3, CI>::operator-(const Vector_<T, VSL_NAMESPACE::D3, CI>& p) const {
        return Vector_<decltype(value.___AN1 - p.value.___AN1), VSL_NAMESPACE::D3, CI>(value.___AN1 - p.value.___AN1, value.___AN2 - p.value.___AN2, value.___AN3 - p.value.___AN3);
    }

    template<typename R, typename CI>
    template<typename T>
    requires(!concepts::is_vector<T>) && concepts::multiply_as<const R&, const T&>
    constexpr auto Vector_<R, VSL_NAMESPACE::D3, CI>::operator*(const T& m) const {
        return Vector_<decltype(value.___AN1 * m), VSL_NAMESPACE::D3, CI>(value.___AN1 * m, value.___AN2 * m, value.___AN3 * m);
    }

    template<typename R, typename CI>
    template<typename T>
    requires concepts::division_as<const R&, const T&>
    constexpr auto Vector_<R, VSL_NAMESPACE::D3, CI>::operator/(const T& m) const {
        return Vector_<decltype(value.___AN1 / m), VSL_NAMESPACE::D3>(value.___AN1 / m, value.___AN2 / m, value.___AN3 / m);
    }

    template<typename R, typename CI>
    template<typename T>
    requires concepts::add_assign_as<R&, const T&>
    constexpr Vector_<R, VSL_NAMESPACE::D3, CI>& Vector_<R, VSL_NAMESPACE::D3, CI>::operator+=(const Vector_<T, VSL_NAMESPACE::D3, CI>& p) {
        this->value.___AN1 += p.value.___AN1;
        this->value.___AN2 += p.value.___AN2;
        this->value.___AN3 += p.value.___AN3;
        return *this;
    }

    template<typename R, typename CI>
    template<typename T>
    requires concepts::subtract_assign_as<R&, const T&>
    constexpr Vector_<R, VSL_NAMESPACE::D3, CI>& Vector_<R, VSL_NAMESPACE::D3, CI>::operator-=(const Vector_<T, VSL_NAMESPACE::D3, CI>& p) {
        this->value.___AN1 -= p.value.___AN1;
        this->value.___AN2 -= p.value.___AN2;
        this->value.___AN3 -= p.value.___AN3;
        return *this;
    }

    template<typename R, typename CI>
    template<typename T>
    requires concepts::multiply_assign_as<R&, const T&>
    constexpr Vector_<R, VSL_NAMESPACE::D3, CI>& Vector_<R, VSL_NAMESPACE::D3, CI>::operator*=(const T& p) {
        this->value.___AN1 *= p;
        this->value.___AN2 *= p;
        this->value.___AN3 *= p;
        return *this;
    }

    template<typename R, typename CI>
    template<typename T>
    requires concepts::division_assign_as<R&, const T&>
    constexpr Vector_<R, VSL_NAMESPACE::D3, CI>& Vector_<R, VSL_NAMESPACE::D3, CI>::operator/=(const T& p) {
        this->value.___AN1 /= p;
        this->value.___AN2 /= p;
        this->value.___AN3 /= p;
        return *this;
    }

    template<typename R, typename CI>
    template<typename T>
    requires concepts::cast_as<R, T>
    constexpr Vector_<T, VSL_NAMESPACE::D3, CI> Vector_<R, VSL_NAMESPACE::D3, CI>::cast() const {
        return Vector_<T, VSL_NAMESPACE::D3, CI>((T)value.___AN1, (T)value.___AN2, (T)value.___AN3);
    }

    template<typename R, typename CI>
    template<typename T>
    requires concepts::is_vector<T> && std::convertible_to<T, Vector_<typename T::ElementType, VSL_NAMESPACE::D3, CI>>
    inline constexpr auto Vector_<R, VSL_NAMESPACE::D3, CI>::dot(const T& m) const {
        return value.___AN1 * m.value.___AN1 + value.___AN2 * m.value.___AN2 + value.___AN3 * m.value.___AN3;
    }

    template<typename R, typename CI>
    template<typename T>
    requires concepts::is_vector<T> && std::convertible_to<T, Vector_<typename T::ElementType, VSL_NAMESPACE::D3, CI>>
    inline constexpr auto Vector_<R, VSL_NAMESPACE::D3, CI>::cross(const T& m) const {
        using Result = decltype(value.y.value * m.value.z.value - value.z.value * m.value.y.value);
        return Vector_<Result, VSL_NAMESPACE::D3, CI>{
            value.y.value * m.value.z.value - value.z.value * m.value.y.value,
            value.z.value * m.value.x.value - value.x.value * m.value.z.value,
            value.x.value * m.value.y.value - value.y.value * m.value.x.value};
    }

    template<typename R, typename CI>
    template<typename T>
    requires concepts::is_vector<T> && std::convertible_to<T, Vector_<typename T::ElementType, VSL_NAMESPACE::D3, CI>>
    constexpr auto Vector_<R, VSL_NAMESPACE::D3, CI>::hadamard(const T& m) const {
        Vector_<decltype(value.___AN1 * m.value.___AN1), VSL_NAMESPACE::D3, CI> result;
        result.value.___AN1 = value.___AN1 * m.value.___AN1;
        result.value.___AN2 = value.___AN2 * m.value.___AN2;
        result.value.___AN3 = value.___AN3 * m.value.___AN3;
        return result;
    }

    template<typename R, typename CI>
    constexpr auto Vector_<R, VSL_NAMESPACE::D3, CI>::length() const {
        return std::hypot(value.___AN1, value.___AN2, value.___AN3);
    }

    template<typename R, typename CI>
    constexpr Vector_<R, VSL_NAMESPACE::D3, CI>& Vector_<R, VSL_NAMESPACE::D3, CI>::normalize() {
        if constexpr (std::floating_point<R>)
            *this = get_normalized();
        else
            *this /= length();
        return *this;
    }

    template<typename R, typename CI>
    constexpr auto Vector_<R, VSL_NAMESPACE::D3, CI>::get_normalized() const {
        if constexpr (std::floating_point<R>) {
            const auto scale = std::max({std::abs(value.___AN1), std::abs(value.___AN2), std::abs(value.___AN3)});
            if (scale > 0 && std::isfinite(scale)) {
                const auto scaled = *this / scale;
                return scaled / scaled.length();
            }
        }
        return *this / length();
    }

    template<typename R, typename CI>
    constexpr Vector_<R, VSL_NAMESPACE::D3, CI>::operator Mat<R, 1, 4>() const {
        return Mat<R, 1, 4>({{value.___AN1, value.___AN2, value.___AN3, 0}});
    }

    template<typename R, typename CI>
    constexpr Vector_<R, VSL_NAMESPACE::D3, CI>::operator Mat<R, 4, 4>() const {
        return Mat<R, 4, 4>({{value.___AN1, 0, 0, 0}, {0, value.___AN2, 0, 0}, {0, 0, value.___AN3, 0}});
    }

    // ---------------------------------------------------------------------------------------------------------------------

    namespace product::dot {
        template<typename Dim, typename CI, typename R1, typename R2>
        constexpr auto operator*(Vector_<R1, Dim, CI> v1, Vector_<R2, Dim, CI> v2) {
            return v1.dot(v2);
        }
    } // namespace product::dot

    namespace product::cross {
        template<typename Dim, typename CI, typename R1, typename R2>
        constexpr auto operator*(Vector_<R1, Dim, CI> v1, Vector_<R2, Dim, CI> v2) {
            return v1.cross(v2);
        }
    } // namespace product::cross

    namespace product::hadamard {
        template<typename Dim, typename CI, typename R1, typename R2>
        constexpr auto operator*(Vector_<R1, Dim, CI> v1, Vector_<R2, Dim, CI> v2) {
            return v1.hadamard(v2);
        }
    } // namespace product::hadamard

    // ---------------------------------------------------------------------------------------------------------------------

    template<typename L, typename R, typename Dim, typename CI>
    requires(!concepts::is_vector<L>) && concepts::multiply_as<const L&, const R&>
    constexpr auto operator*(const L& l, Vector_<R, Dim, CI> v1) {
        const auto& components = v1.value;
        using Result = Vector_<decltype(l * components.___AN1), Dim, CI>;
        if constexpr (Dim::DIMENSION == 1)
            return Result(l * components.___AN1);
        else if constexpr (Dim::DIMENSION == 2)
            return Result(l * components.___AN1, l * components.___AN2);
        else if constexpr (Dim::DIMENSION == 3)
            return Result(l * components.___AN1, l * components.___AN2, l * components.___AN3);
        else
            return Result(l * components.___AN1, l * components.___AN2, l * components.___AN3, l * components.___AN4);
    }
    template<typename R, typename CI>
    template<typename T>
    requires concepts::add_assign_as<R&, const T&>
    constexpr Vector_<R, VSL_NAMESPACE::D4, CI>& Vector_<R, VSL_NAMESPACE::D4, CI>::operator+=(const Vector_<T, Dimension, CI>& p) {
        value.___AN1 += p.value.___AN1;
        value.___AN2 += p.value.___AN2;
        value.___AN3 += p.value.___AN3;
        value.___AN4 += p.value.___AN4;
        return *this;
    }

    template<typename R, typename CI>
    template<typename T>
    requires concepts::subtract_assign_as<R&, const T&>
    constexpr Vector_<R, VSL_NAMESPACE::D4, CI>& Vector_<R, VSL_NAMESPACE::D4, CI>::operator-=(const Vector_<T, Dimension, CI>& p) {
        value.___AN1 -= p.value.___AN1;
        value.___AN2 -= p.value.___AN2;
        value.___AN3 -= p.value.___AN3;
        value.___AN4 -= p.value.___AN4;
        return *this;
    }

    template<typename R, typename CI>
    template<typename T>
    requires(!concepts::is_vector<T>) && concepts::multiply_assign_as<R&, const T&>
    constexpr Vector_<R, VSL_NAMESPACE::D4, CI>& Vector_<R, VSL_NAMESPACE::D4, CI>::operator*=(const T& m) {
        value.___AN1 *= m;
        value.___AN2 *= m;
        value.___AN3 *= m;
        value.___AN4 *= m;
        return *this;
    }

    template<typename R, typename CI>
    template<typename T>
    requires(!concepts::is_vector<T>) && concepts::division_assign_as<R&, const T&>
    constexpr Vector_<R, VSL_NAMESPACE::D4, CI>& Vector_<R, VSL_NAMESPACE::D4, CI>::operator/=(const T& m) {
        value.___AN1 /= m;
        value.___AN2 /= m;
        value.___AN3 /= m;
        value.___AN4 /= m;
        return *this;
    }

    template<typename R, typename CI>
    Vector_<R, VSL_NAMESPACE::D4, CI>& Vector_<R, VSL_NAMESPACE::D4, CI>::normalize()
    requires std::floating_point<R>
    {
        const auto scale = std::max({std::abs(value.___AN1), std::abs(value.___AN2), std::abs(value.___AN3), std::abs(value.___AN4)});
        if (!std::isfinite(scale) || scale == 0)
            throw std::invalid_argument("Cannot normalize a zero or non-finite vector");
        auto scaled = *this / scale;
        const auto norm = scaled.length();
        if (!std::isfinite(norm))
            throw std::invalid_argument("Cannot normalize a non-finite vector");
        *this = scaled / norm;
        return *this;
    }

} // namespace AHO_NAMESPACE
