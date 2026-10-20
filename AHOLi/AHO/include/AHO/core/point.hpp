#pragma once
#include "../define.hpp"
#include "../concepts.hpp"

#include "VSL/dimension.hpp"
#include "VSL/format.hpp"

#include "math/coordinate.hpp"

#include "vector.hpp"

#include <cmath>
#include <type_traits>

namespace AHO_NAMESPACE {
    template<typename R, VSL_NAMESPACE::is_dimension D, typename CoordinateInfo>
    struct _Point {
    };

    namespace concepts {
        template<typename T>
        concept is_point = requires(T t) {
            typename std::remove_reference_t<T>::dimension;
            typename std::remove_reference_t<T>::element_type;
            { std::convertible_to<std::remove_reference_t<T>, _Point<typename std::remove_reference_t<T>::element_type, typename std::remove_reference_t<T>::dimension, typename std::remove_reference_t<T>::coordinate_info>> };
            (_Point<typename std::remove_reference_t<T>::element_type, typename std::remove_reference_t<T>::dimension, typename std::remove_reference_t<T>::coordinate_info>)t;
        };
    } // namespace concepts

    template<typename R = int, typename... Args>
    requires concepts::sames_as<R, Args...>
    struct Point : public _Point<R,
                                 typename VSL_NAMESPACE::dimension<sizeof...(Args) + 1>::value,
                                 typename AHO_NAMESPACE::coordinate::_DefaultCoordinateInfo<
                                     typename VSL_NAMESPACE::dimension<sizeof...(Args) + 1>::value>::value> {
        constexpr Point() {};

        constexpr Point(R x, Args... args) : _Point<R, typename VSL_NAMESPACE::dimension<sizeof...(Args) + 1>::value>(x, args...) {};
    };

    namespace d1 {
        template<typename R, typename CI = typename AHO_NAMESPACE::coordinate::_DefaultCoordinateInfo<VSL_NAMESPACE::D1>::value>
        struct Point : public AHO_NAMESPACE::_Point<R, VSL_NAMESPACE::D1, CI> {
            constexpr Point() {};

            constexpr Point(R x) : AHO_NAMESPACE::_Point<R, VSL_NAMESPACE::D1, CI>(x) {};
        };
        using PointI = AHO_NAMESPACE::_Point<int, VSL_NAMESPACE::D1>;
        using PointF = AHO_NAMESPACE::_Point<float, VSL_NAMESPACE::D1>;
        using PointD = AHO_NAMESPACE::_Point<double, VSL_NAMESPACE::D1>;
    } // namespace d1

    namespace d2 {
        template<typename R, typename CI = typename AHO_NAMESPACE::coordinate::_DefaultCoordinateInfo<VSL_NAMESPACE::D2>::value>
        struct Point : public AHO_NAMESPACE::_Point<R, VSL_NAMESPACE::D2, CI> {
            constexpr Point() {};

            constexpr Point(R x, R y) : AHO_NAMESPACE::_Point<R, VSL_NAMESPACE::D2, CI>(x, y) {};
        };
        using PointI = AHO_NAMESPACE::_Point<int, VSL_NAMESPACE::D2>;
        using PointF = AHO_NAMESPACE::_Point<float, VSL_NAMESPACE::D2>;
        using PointD = AHO_NAMESPACE::_Point<double, VSL_NAMESPACE::D2>;
    } // namespace d2

    namespace d3 {
        template<typename R, typename CI = typename AHO_NAMESPACE::coordinate::_DefaultCoordinateInfo<VSL_NAMESPACE::D3>::value>
        struct Point : public AHO_NAMESPACE::_Point<R, VSL_NAMESPACE::D3, CI> {
            constexpr Point() {};

            constexpr Point(R x, R y, R z) : AHO_NAMESPACE::_Point<R, VSL_NAMESPACE::D3, CI>(x, y, z) {};
        };
        using PointI = AHO_NAMESPACE::_Point<int, VSL_NAMESPACE::D3>;
        using PointF = AHO_NAMESPACE::_Point<float, VSL_NAMESPACE::D3>;
        using PointD = AHO_NAMESPACE::_Point<double, VSL_NAMESPACE::D3>;
    } // namespace d3

    template<typename R, typename CI>
    struct _Point<R, VSL_NAMESPACE::D1, CI> {
        using dimension = typename VSL_NAMESPACE::D1;
        using element_type = R;
        using coordinate_info = CI;

        constexpr static VSL_NAMESPACE::data_format::___Format graphic_type = VSL_NAMESPACE::data_format::convert_graphic_type<R>()
                                                                                  .template toVec<(size_t)dimension::dimension>();

        coordinate::_CoordinateSet<R, CI> value;

        constexpr _Point() {};

        constexpr _Point(R value) : value(value) {};

        constexpr _Point(coordinate::_CoordinateSet<R, CI> value) : value(value) {};

        constexpr bool operator<(const _Point<R, VSL_NAMESPACE::D1, CI>& p) const;

        constexpr auto operator<=>(const _Point<R, VSL_NAMESPACE::D1, CI>&) const = default;

        constexpr _Point<R, VSL_NAMESPACE::D1, CI> operator+() const;

        constexpr _Point<R, VSL_NAMESPACE::D1, CI> operator-() const;

        template<typename T>
        requires requires(const R& lhs, const T& rhs) { lhs - rhs; }
        constexpr auto /*Vector*/ operator-(const _Point<T, VSL_NAMESPACE::D1, CI>& p) const;

        template<typename T>
        requires requires(const R& lhs, const T& rhs) { lhs + rhs; }
        constexpr auto /*Point*/ operator+(const _Vector<T, VSL_NAMESPACE::D1, CI>& p) const;

        template<typename T>
        requires requires(const R& lhs, const T& rhs) { lhs - rhs; }
        constexpr auto /*Point*/ operator-(const _Vector<T, VSL_NAMESPACE::D1, CI>& p) const;

        template<typename T>
        requires requires(const R& lhs, const T& rhs) { lhs * rhs; }
        constexpr auto /*Point*/ operator*(const T& m) const;

        template<typename T>
        requires requires(const R& lhs, const T& rhs) { lhs / rhs; }
        constexpr auto /*Point*/ operator/(const T& m) const;

        template<typename T>
        requires requires(R& lhs, const T& rhs) { lhs += rhs; }
        constexpr _Point<R, VSL_NAMESPACE::D1, CI>& /*Point*/ operator+=(const _Vector<T, VSL_NAMESPACE::D1, CI>& p);

        template<typename T>
        requires requires(R& lhs, const T& rhs) { lhs -= rhs; }
        constexpr _Point<R, VSL_NAMESPACE::D1, CI>& /*Point*/ operator-=(const _Vector<T, VSL_NAMESPACE::D1, CI>& p);

        template<typename T>
        requires requires(R& lhs, const T& rhs) { lhs *= rhs; }
        constexpr _Point<R, VSL_NAMESPACE::D1, CI>& /*Point*/ operator*=(const T& m);

        template<typename T>
        requires requires(R& lhs, const T& rhs) { lhs /= rhs; }
        constexpr _Point<R, VSL_NAMESPACE::D1, CI>& /*Point*/ operator/=(const T& m);

        template<typename T>
        requires concepts::cast_as<R, T>
        constexpr _Point<T, VSL_NAMESPACE::D1, CI> cast() const;

        constexpr auto length() const;
    };

    template<typename R, typename CI>
    struct _Point<R, VSL_NAMESPACE::D2, CI> {
        using dimension = typename VSL_NAMESPACE::D2;
        using element_type = R;
        using coordinate_info = CI;

        static constexpr VSL_NAMESPACE::data_format::___Format graphic_type = VSL_NAMESPACE::data_format::convert_graphic_type<R>().template toVec<(size_t)dimension::dimension>();

        coordinate::_CoordinateSet<R, CI> value;

        constexpr _Point() {};

        constexpr _Point(R v1, R v2) : value(v1, v2) {};

        constexpr _Point(coordinate::_CoordinateSet<R, CI> value) : value(value) {};

        constexpr bool operator<(const _Point<R, VSL_NAMESPACE::D2, CI>& p) const;

        constexpr auto operator<=>(const _Point<R, VSL_NAMESPACE::D2, CI>&) const = default;

        constexpr _Point<R, VSL_NAMESPACE::D2, CI> operator+() const;

        constexpr _Point<R, VSL_NAMESPACE::D2, CI> operator-() const;

        template<typename T>
        requires requires(const R& lhs, const T& rhs) { lhs - rhs; }
        constexpr auto /*Vector*/ operator-(const _Point<T, VSL_NAMESPACE::D2, CI>& p) const;

        template<typename T>
        requires requires(const R& lhs, const T& rhs) { lhs + rhs; }
        constexpr auto /*Point*/ operator+(const _Vector<T, VSL_NAMESPACE::D2, CI>& p) const;

        template<typename T>
        requires requires(const R& lhs, const T& rhs) { lhs - rhs; }
        constexpr auto /*Point*/ operator-(const _Vector<T, VSL_NAMESPACE::D2, CI>& p) const;

        template<typename T>
        requires requires(const R& lhs, const T& rhs) { lhs * rhs; }
        constexpr auto /*Point*/ operator*(const T& m) const;

        template<typename T>
        requires requires(const R& lhs, const T& rhs) { lhs / rhs; }
        constexpr auto /*Point*/ operator/(const T& m) const;

        template<typename T>
        requires requires(R& lhs, const T& rhs) { lhs += rhs; }
        constexpr _Point<R, VSL_NAMESPACE::D2, CI>& /*Point*/ operator+=(const _Vector<T, VSL_NAMESPACE::D2, CI>& p);

        template<typename T>
        requires requires(R& lhs, const T& rhs) { lhs -= rhs; }
        constexpr _Point<R, VSL_NAMESPACE::D2, CI>& /*Point*/ operator-=(const _Vector<T, VSL_NAMESPACE::D2, CI>& p);

        template<typename T>
        requires requires(R& lhs, const T& rhs) { lhs *= rhs; }
        constexpr _Point<R, VSL_NAMESPACE::D2, CI>& /*Point*/ operator*=(const T& m);

        template<typename T>
        requires requires(R& lhs, const T& rhs) { lhs /= rhs; }
        constexpr _Point<R, VSL_NAMESPACE::D2, CI>& /*Point*/ operator/=(const T& m);

        template<typename T>
        requires concepts::cast_as<R, T>
        constexpr _Point<T, VSL_NAMESPACE::D2, CI> cast() const;

        constexpr auto length() const;
    };

    template<typename R, typename CI>
    struct _Point<R, VSL_NAMESPACE::D3, CI> {
        using dimension = typename VSL_NAMESPACE::D3;
        using element_type = R;
        using coordinate_info = CI;

        static constexpr VSL_NAMESPACE::data_format::___Format graphic_type = VSL_NAMESPACE::data_format::convert_graphic_type<R>().template toVec<(size_t)dimension::dimension>();

        coordinate::_CoordinateSet<R, CI> value;

        constexpr _Point() {};

        constexpr _Point(R x, R y, R z) : value(x, y, z) {};

        constexpr _Point(coordinate::_CoordinateSet<R, CI> value) : value(value) {};

        constexpr bool operator<(const _Point<R, VSL_NAMESPACE::D3, CI>& p) const;

        constexpr auto operator<=>(const _Point<R, VSL_NAMESPACE::D3, CI>&) const = default;

        constexpr _Point<R, VSL_NAMESPACE::D3, CI> operator+() const;

        constexpr _Point<R, VSL_NAMESPACE::D3, CI> operator-() const;

        template<typename T>
        requires requires(const R& lhs, const T& rhs) { lhs - rhs; }
        constexpr auto /*Vector*/ operator-(const _Point<T, VSL_NAMESPACE::D3, CI>& p) const;

        template<typename T>
        requires requires(const R& lhs, const T& rhs) { lhs + rhs; }
        constexpr auto /*Point*/ operator+(const _Vector<T, VSL_NAMESPACE::D3, CI>& p) const;

        template<typename T>
        requires requires(const R& lhs, const T& rhs) { lhs - rhs; }
        constexpr auto /*Point*/ operator-(const _Vector<T, VSL_NAMESPACE::D3, CI>& p) const;

        template<typename T>
        requires requires(const R& lhs, const T& rhs) { lhs * rhs; }
        constexpr auto /*Point*/ operator*(const T& m) const;

        template<typename T>
        requires requires(const R& lhs, const T& rhs) { lhs / rhs; }
        constexpr auto /*Point*/ operator/(const T& m) const;

        template<typename T>
        requires requires(R& lhs, const T& rhs) { lhs += rhs; }
        constexpr _Point<R, VSL_NAMESPACE::D3, CI>& /*Point*/ operator+=(const _Vector<T, VSL_NAMESPACE::D3, CI>& p);

        template<typename T>
        requires requires(R& lhs, const T& rhs) { lhs -= rhs; }
        constexpr _Point<R, VSL_NAMESPACE::D3, CI>& /*Point*/ operator-=(const _Vector<T, VSL_NAMESPACE::D3, CI>& p);

        template<typename T>
        requires requires(R& lhs, const T& rhs) { lhs *= rhs; }
        constexpr _Point<R, VSL_NAMESPACE::D3, CI>& /*Point*/ operator*=(const T& m);

        template<typename T>
        requires requires(R& lhs, const T& rhs) { lhs /= rhs; }
        constexpr _Point<R, VSL_NAMESPACE::D3, CI>& /*Point*/ operator/=(const T& m);

        template<typename T>
        requires concepts::cast_as<R, T>
        constexpr _Point<T, VSL_NAMESPACE::D3, CI> cast() const;

        constexpr auto length() const;
    };

    template<typename R, typename CI>
    _Point(coordinate::_CoordinateSet<R, CI>)
        -> _Point<R, typename CI::dimension, CI>;

    // ---------------------------------------------------------------------------------------------------------------------

    template<typename R, typename CI>
    constexpr bool _Point<R, VSL_NAMESPACE::D1, CI>::operator<(const _Point<R, VSL_NAMESPACE::D1, CI>& p) const {
        return length() < p.length();
    }

    template<typename R, typename CI>
    constexpr _Point<R, VSL_NAMESPACE::D1, CI> _Point<R, VSL_NAMESPACE::D1, CI>::operator+() const {
        return *this;
    }

    template<typename R, typename CI>
    constexpr _Point<R, VSL_NAMESPACE::D1, CI> _Point<R, VSL_NAMESPACE::D1, CI>::operator-() const {
        return _Point<R, VSL_NAMESPACE::D1, CI>(-value.___AN1);
    }

    template<typename R, typename CI>
    template<typename T>
    requires requires(const R& lhs, const T& rhs) { lhs - rhs; }
    constexpr auto _Point<R, VSL_NAMESPACE::D1, CI>::operator-(const _Point<T, VSL_NAMESPACE::D1, CI>& p) const {
        return _Vector<decltype(value.___AN1 - p.value.___AN1), VSL_NAMESPACE::D1, CI>(value.___AN1 - p.value.___AN1);
    }

    template<typename R, typename CI>
    template<typename T>
    requires requires(const R& lhs, const T& rhs) { lhs + rhs; }
    constexpr auto _Point<R, VSL_NAMESPACE::D1, CI>::operator+(const _Vector<T, VSL_NAMESPACE::D1, CI>& p) const {
        return _Point<decltype(value.___AN1 + p.value.___AN1), VSL_NAMESPACE::D1, CI>(value.___AN1 + p.value.___AN1);
    }

    template<typename R, typename CI>
    template<typename T>
    requires requires(const R& lhs, const T& rhs) { lhs - rhs; }
    constexpr auto _Point<R, VSL_NAMESPACE::D1, CI>::operator-(const _Vector<T, VSL_NAMESPACE::D1, CI>& p) const {
        return _Point<decltype(value.___AN1 - p.value.___AN1), VSL_NAMESPACE::D1, CI>(value.___AN1 - p.value.___AN1);
    }

    template<typename R, typename CI>
    template<typename T>
    requires requires(const R& lhs, const T& rhs) { lhs * rhs; }
    constexpr auto _Point<R, VSL_NAMESPACE::D1, CI>::operator*(const T& m) const {
        return _Point<decltype(value.___AN1 * m), VSL_NAMESPACE::D1, CI>(value.___AN1 * m);
    }

    template<typename R, typename CI>
    template<typename T>
    requires requires(const R& lhs, const T& rhs) { lhs / rhs; }
    constexpr auto _Point<R, VSL_NAMESPACE::D1, CI>::operator/(const T& m) const {
        return _Point<decltype(value.___AN1 / m), VSL_NAMESPACE::D1, CI>(value.___AN1 / m);
    }

    template<typename R, typename CI>
    template<typename T>
    requires requires(R& lhs, const T& rhs) { lhs += rhs; }
    constexpr _Point<R, VSL_NAMESPACE::D1, CI>& _Point<R, VSL_NAMESPACE::D1, CI>::operator+=(const _Vector<T, VSL_NAMESPACE::D1, CI>& p) {
        this->value.___AN1 += p.value.___AN1;
        return *this;
    }

    template<typename R, typename CI>
    template<typename T>
    requires requires(R& lhs, const T& rhs) { lhs -= rhs; }
    constexpr _Point<R, VSL_NAMESPACE::D1, CI>& _Point<R, VSL_NAMESPACE::D1, CI>::operator-=(const _Vector<T, VSL_NAMESPACE::D1, CI>& p) {
        this->value.___AN1 -= p.value.___AN1;
        return *this;
    }

    template<typename R, typename CI>
    template<typename T>
    requires requires(R& lhs, const T& rhs) { lhs *= rhs; }
    constexpr _Point<R, VSL_NAMESPACE::D1, CI>& _Point<R, VSL_NAMESPACE::D1, CI>::operator*=(const T& m) {
        this->value.___AN1 *= m;
        return *this;
    }

    template<typename R, typename CI>
    template<typename T>
    requires requires(R& lhs, const T& rhs) { lhs /= rhs; }
    constexpr _Point<R, VSL_NAMESPACE::D1, CI>& _Point<R, VSL_NAMESPACE::D1, CI>::operator/=(const T& m) {
        this->value.___AN1 /= m;
        return *this;
    }

    template<typename R, typename CI>
    template<typename T>
    requires concepts::cast_as<R, T>
    constexpr _Point<T, VSL_NAMESPACE::D1, CI> _Point<R, VSL_NAMESPACE::D1, CI>::cast() const {
        return _Point<T, VSL_NAMESPACE::D1, CI>((T)value.___AN1);
    }

    template<typename R, typename CI>
    constexpr auto _Point<R, VSL_NAMESPACE::D1, CI>::length() const {
        const R component = [&]() {
            if constexpr (CI::EnabledX)
                return value.x.value;
            else if constexpr (CI::EnabledY)
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

    // ---------------------------------------------------------------------------------------------------------------------

    template<typename R, typename CI>
    constexpr bool _Point<R, VSL_NAMESPACE::D2, CI>::operator<(const _Point<R, VSL_NAMESPACE::D2, CI>& p) const {
        return value < p.value;
    }

    template<typename R, typename CI>
    constexpr _Point<R, VSL_NAMESPACE::D2, CI> _Point<R, VSL_NAMESPACE::D2, CI>::operator+() const {
        return *this;
    }

    template<typename R, typename CI>
    constexpr _Point<R, VSL_NAMESPACE::D2, CI> _Point<R, VSL_NAMESPACE::D2, CI>::operator-() const {
        return _Point<R, VSL_NAMESPACE::D2, CI>(-value.___AN1, -value.___AN2);
    }

    template<typename R, typename CI>
    template<typename T>
    requires requires(const R& lhs, const T& rhs) { lhs - rhs; }
    constexpr auto _Point<R, VSL_NAMESPACE::D2, CI>::operator-(const _Point<T, VSL_NAMESPACE::D2, CI>& p) const {
        return _Vector<decltype(value.___AN1 - p.value.___AN1), VSL_NAMESPACE::D2, CI>(value.___AN1 - p.value.___AN1, value.___AN2 - p.value.___AN2);
    }

    template<typename R, typename CI>
    template<typename T>
    requires requires(const R& lhs, const T& rhs) { lhs + rhs; }
    constexpr auto _Point<R, VSL_NAMESPACE::D2, CI>::operator+(const _Vector<T, VSL_NAMESPACE::D2, CI>& p) const {
        return _Point<decltype(value.___AN1 + p.value.___AN1), VSL_NAMESPACE::D2, CI>(value.___AN1 + p.value.___AN1, value.___AN2 + p.value.___AN2);
    }

    template<typename R, typename CI>
    template<typename T>
    requires requires(const R& lhs, const T& rhs) { lhs - rhs; }
    constexpr auto _Point<R, VSL_NAMESPACE::D2, CI>::operator-(const _Vector<T, VSL_NAMESPACE::D2, CI>& p) const {
        return _Point<decltype(value.___AN1 - p.value.___AN1), VSL_NAMESPACE::D2, CI>(value.___AN1 - p.value.___AN1, value.___AN2 - p.value.___AN2);
    }

    template<typename R, typename CI>
    template<typename T>
    requires requires(const R& lhs, const T& rhs) { lhs * rhs; }
    constexpr auto _Point<R, VSL_NAMESPACE::D2, CI>::operator*(const T& m) const {
        return _Point<decltype(value.___AN1 * m), VSL_NAMESPACE::D2, CI>(value.___AN1 * m, value.___AN2 * m);
    }

    template<typename R, typename CI>
    template<typename T>
    requires requires(const R& lhs, const T& rhs) { lhs / rhs; }
    constexpr auto _Point<R, VSL_NAMESPACE::D2, CI>::operator/(const T& m) const {
        return _Point<decltype(value.___AN1 / m), VSL_NAMESPACE::D2, CI>(value.___AN1 / m, value.___AN2 / m);
    }

    template<typename R, typename CI>
    template<typename T>
    requires requires(R& lhs, const T& rhs) { lhs += rhs; }
    constexpr _Point<R, VSL_NAMESPACE::D2, CI>& _Point<R, VSL_NAMESPACE::D2, CI>::operator+=(const _Vector<T, VSL_NAMESPACE::D2, CI>& p) {
        this->value.___AN1 += p.value.___AN1;
        this->value.___AN2 += p.value.___AN2;
        return *this;
    }

    template<typename R, typename CI>
    template<typename T>
    requires requires(R& lhs, const T& rhs) { lhs -= rhs; }
    constexpr _Point<R, VSL_NAMESPACE::D2, CI>& _Point<R, VSL_NAMESPACE::D2, CI>::operator-=(const _Vector<T, VSL_NAMESPACE::D2, CI>& p) {
        this->value.___AN1 -= p.value.___AN1;
        this->value.___AN2 -= p.value.___AN2;
        return *this;
    }

    template<typename R, typename CI>
    template<typename T>
    requires requires(R& lhs, const T& rhs) { lhs *= rhs; }
    constexpr _Point<R, VSL_NAMESPACE::D2, CI>& _Point<R, VSL_NAMESPACE::D2, CI>::operator*=(const T& m) {
        this->value.___AN1 *= m;
        this->value.___AN2 *= m;
        return *this;
    }

    template<typename R, typename CI>
    template<typename T>
    requires requires(R& lhs, const T& rhs) { lhs /= rhs; }
    constexpr _Point<R, VSL_NAMESPACE::D2, CI>& _Point<R, VSL_NAMESPACE::D2, CI>::operator/=(const T& m) {
        this->value.___AN1 /= m;
        this->value.___AN2 /= m;
        return *this;
    }

    template<typename R, typename CI>
    template<typename T>
    requires concepts::cast_as<R, T>
    constexpr _Point<T, VSL_NAMESPACE::D2, CI> _Point<R, VSL_NAMESPACE::D2, CI>::cast() const {
        return _Point<T, VSL_NAMESPACE::D2, CI>((T)value.___AN1, (T)value.___AN2);
    }

    template<typename R, typename CI>
    constexpr auto _Point<R, VSL_NAMESPACE::D2, CI>::length() const {
        return std::hypot(value.___AN1, value.___AN2);
    }

    // ---------------------------------------------------------------------------------------------------------------------

    template<typename R, typename CI>
    constexpr bool _Point<R, VSL_NAMESPACE::D3, CI>::operator<(const _Point<R, VSL_NAMESPACE::D3, CI>& p) const {
        return length() < p.length();
    }

    template<typename R, typename CI>
    constexpr _Point<R, VSL_NAMESPACE::D3, CI> _Point<R, VSL_NAMESPACE::D3, CI>::operator+() const {
        return *this;
    }

    template<typename R, typename CI>
    constexpr _Point<R, VSL_NAMESPACE::D3, CI> _Point<R, VSL_NAMESPACE::D3, CI>::operator-() const {
        return _Point<R, VSL_NAMESPACE::D3, CI>(-value.___AN1, -value.___AN2, -value.___AN3);
    }

    template<typename R, typename CI>
    template<typename T>
    requires requires(const R& lhs, const T& rhs) { lhs - rhs; }
    constexpr auto _Point<R, VSL_NAMESPACE::D3, CI>::operator-(const _Point<T, VSL_NAMESPACE::D3, CI>& p) const {
        return _Vector<decltype(value.___AN1 - p.value.___AN1), VSL_NAMESPACE::D3, CI>(value.___AN1 - p.value.___AN1, value.___AN2 - p.value.___AN2, value.___AN3 - p.value.___AN3);
    }

    template<typename R, typename CI>
    template<typename T>
    requires requires(const R& lhs, const T& rhs) { lhs + rhs; }
    constexpr auto _Point<R, VSL_NAMESPACE::D3, CI>::operator+(const _Vector<T, VSL_NAMESPACE::D3, CI>& p) const {
        return _Point<decltype(value.___AN1 + p.value.___AN1), VSL_NAMESPACE::D3, CI>(value.___AN1 + p.value.___AN1, value.___AN2 + p.value.___AN2, value.___AN3 + p.value.___AN3);
    }

    template<typename R, typename CI>
    template<typename T>
    requires requires(const R& lhs, const T& rhs) { lhs - rhs; }
    constexpr auto _Point<R, VSL_NAMESPACE::D3, CI>::operator-(const _Vector<T, VSL_NAMESPACE::D3, CI>& p) const {
        return _Point<decltype(value.___AN1 - p.value.___AN1), VSL_NAMESPACE::D3, CI>(value.___AN1 - p.value.___AN1, value.___AN2 - p.value.___AN2, value.___AN3 - p.value.___AN3);
    }

    template<typename R, typename CI>
    template<typename T>
    requires requires(const R& lhs, const T& rhs) { lhs * rhs; }
    constexpr auto _Point<R, VSL_NAMESPACE::D3, CI>::operator*(const T& m) const {
        return _Point<decltype(value.___AN1 * m), VSL_NAMESPACE::D3, CI>(value.___AN1 * m, value.___AN2 * m, value.___AN3 * m);
    }

    template<typename R, typename CI>
    template<typename T>
    requires requires(const R& lhs, const T& rhs) { lhs / rhs; }
    constexpr auto _Point<R, VSL_NAMESPACE::D3, CI>::operator/(const T& m) const {
        return _Point<decltype(value.___AN1 / m), VSL_NAMESPACE::D3, CI>(value.___AN1 / m, value.___AN2 / m, value.___AN3 / m);
    }

    template<typename R, typename CI>
    template<typename T>
    requires requires(R& lhs, const T& rhs) { lhs += rhs; }
    constexpr _Point<R, VSL_NAMESPACE::D3, CI>& _Point<R, VSL_NAMESPACE::D3, CI>::operator+=(const _Vector<T, VSL_NAMESPACE::D3, CI>& p) {
        this->value.___AN1 += p.value.___AN1;
        this->value.___AN2 += p.value.___AN2;
        this->value.___AN3 += p.value.___AN3;
        return *this;
    }

    template<typename R, typename CI>
    template<typename T>
    requires requires(R& lhs, const T& rhs) { lhs -= rhs; }
    constexpr _Point<R, VSL_NAMESPACE::D3, CI>& _Point<R, VSL_NAMESPACE::D3, CI>::operator-=(const _Vector<T, VSL_NAMESPACE::D3, CI>& p) {
        this->value.___AN1 -= p.value.___AN1;
        this->value.___AN2 -= p.value.___AN2;
        this->value.___AN3 -= p.value.___AN3;
        return *this;
    }

    template<typename R, typename CI>
    template<typename T>
    requires requires(R& lhs, const T& rhs) { lhs *= rhs; }
    constexpr _Point<R, VSL_NAMESPACE::D3, CI>& _Point<R, VSL_NAMESPACE::D3, CI>::operator*=(const T& m) {
        this->value.___AN1 *= m;
        this->value.___AN2 *= m;
        this->value.___AN3 *= m;
        return *this;
    }

    template<typename R, typename CI>
    template<typename T>
    requires requires(R& lhs, const T& rhs) { lhs /= rhs; }
    constexpr _Point<R, VSL_NAMESPACE::D3, CI>& _Point<R, VSL_NAMESPACE::D3, CI>::operator/=(const T& m) {
        this->value.___AN1 /= m;
        this->value.___AN2 /= m;
        this->value.___AN3 /= m;
        return *this;
    }

    template<typename R, typename CI>
    template<typename T>
    requires concepts::cast_as<R, T>
    constexpr _Point<T, VSL_NAMESPACE::D3, CI> _Point<R, VSL_NAMESPACE::D3, CI>::cast() const {
        return _Point<T, VSL_NAMESPACE::D3, CI>((T)value.___AN1, (T)value.___AN2, (T)value.___AN3);
    }

    template<typename R, typename CI>
    constexpr auto _Point<R, VSL_NAMESPACE::D3, CI>::length() const {
        return std::hypot(value.___AN1, value.___AN2, value.___AN3);
    }
} // namespace AHO_NAMESPACE
