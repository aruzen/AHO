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
    struct Point_ {
    };

    namespace concepts {
        template<typename T>
        concept is_point = requires(T t) {
            typename std::remove_reference_t<T>::Dimension;
            typename std::remove_reference_t<T>::ElementType;
            { std::convertible_to<std::remove_reference_t<T>, Point_<typename std::remove_reference_t<T>::ElementType, typename std::remove_reference_t<T>::Dimension, typename std::remove_reference_t<T>::CoordinateInfo>> };
            (Point_<typename std::remove_reference_t<T>::ElementType, typename std::remove_reference_t<T>::Dimension, typename std::remove_reference_t<T>::CoordinateInfo>)t;
        };
    } // namespace concepts

    template<typename R = int, typename... Args>
    requires concepts::sames_as<R, Args...>
    struct Point : public Point_<R,
                                 typename VSL_NAMESPACE::Dimension<sizeof...(Args) + 1>::Value,
                                 typename AHO_NAMESPACE::coordinate::DefaultCoordinateInfo<
                                     typename VSL_NAMESPACE::Dimension<sizeof...(Args) + 1>::Value>::Value> {
        constexpr Point() {};

        constexpr Point(R x, Args... args) : Point_<R, typename VSL_NAMESPACE::Dimension<sizeof...(Args) + 1>::Value>(x, args...) {};
    };

    namespace d1 {
        template<typename R, typename CI = typename AHO_NAMESPACE::coordinate::DefaultCoordinateInfo<VSL_NAMESPACE::D1>::Value>
        struct Point : public AHO_NAMESPACE::Point_<R, VSL_NAMESPACE::D1, CI> {
            constexpr Point() {};

            constexpr Point(R x) : AHO_NAMESPACE::Point_<R, VSL_NAMESPACE::D1, CI>(x) {};
        };
        using PointI = AHO_NAMESPACE::Point_<int, VSL_NAMESPACE::D1>;
        using PointF = AHO_NAMESPACE::Point_<float, VSL_NAMESPACE::D1>;
        using PointD = AHO_NAMESPACE::Point_<double, VSL_NAMESPACE::D1>;
    } // namespace d1

    namespace d2 {
        template<typename R, typename CI = typename AHO_NAMESPACE::coordinate::DefaultCoordinateInfo<VSL_NAMESPACE::D2>::Value>
        struct Point : public AHO_NAMESPACE::Point_<R, VSL_NAMESPACE::D2, CI> {
            constexpr Point() {};

            constexpr Point(R x, R y) : AHO_NAMESPACE::Point_<R, VSL_NAMESPACE::D2, CI>(x, y) {};
        };
        using PointI = AHO_NAMESPACE::Point_<int, VSL_NAMESPACE::D2>;
        using PointF = AHO_NAMESPACE::Point_<float, VSL_NAMESPACE::D2>;
        using PointD = AHO_NAMESPACE::Point_<double, VSL_NAMESPACE::D2>;
    } // namespace d2

    namespace d3 {
        template<typename R, typename CI = typename AHO_NAMESPACE::coordinate::DefaultCoordinateInfo<VSL_NAMESPACE::D3>::Value>
        struct Point : public AHO_NAMESPACE::Point_<R, VSL_NAMESPACE::D3, CI> {
            constexpr Point() {};

            constexpr Point(R x, R y, R z) : AHO_NAMESPACE::Point_<R, VSL_NAMESPACE::D3, CI>(x, y, z) {};
        };
        using PointI = AHO_NAMESPACE::Point_<int, VSL_NAMESPACE::D3>;
        using PointF = AHO_NAMESPACE::Point_<float, VSL_NAMESPACE::D3>;
        using PointD = AHO_NAMESPACE::Point_<double, VSL_NAMESPACE::D3>;
    } // namespace d3

    template<typename R, typename CI>
    struct Point_<R, VSL_NAMESPACE::D1, CI> {
        using Dimension = typename VSL_NAMESPACE::D1;
        using ElementType = R;
        using CoordinateInfo = CI;

        constexpr static VSL_NAMESPACE::data_format::Format GRAPHIC_TYPE = VSL_NAMESPACE::data_format::convert_graphic_type<R>()
                                                                                  .template to_vec<(size_t)Dimension::DIMENSION>();

        coordinate::CoordinateSet<R, CI> value;

        constexpr Point_() {};

        constexpr Point_(R value) : value(value) {};

        constexpr Point_(coordinate::CoordinateSet<R, CI> value) : value(value) {};

        constexpr bool operator<(const Point_<R, VSL_NAMESPACE::D1, CI>& p) const;

        constexpr auto operator<=>(const Point_<R, VSL_NAMESPACE::D1, CI>&) const = default;

        constexpr Point_<R, VSL_NAMESPACE::D1, CI> operator+() const;

        constexpr Point_<R, VSL_NAMESPACE::D1, CI> operator-() const;

        template<typename T>
        requires concepts::subtract_as<const R&, const T&>
        constexpr auto /*Vector*/ operator-(const Point_<T, VSL_NAMESPACE::D1, CI>& p) const;

        template<typename T>
        requires concepts::add_as<const R&, const T&>
        constexpr auto /*Point*/ operator+(const Vector_<T, VSL_NAMESPACE::D1, CI>& p) const;

        template<typename T>
        requires concepts::subtract_as<const R&, const T&>
        constexpr auto /*Point*/ operator-(const Vector_<T, VSL_NAMESPACE::D1, CI>& p) const;

        template<typename T>
        requires concepts::multiply_as<const R&, const T&>
        constexpr auto /*Point*/ operator*(const T& m) const;

        template<typename T>
        requires concepts::division_as<const R&, const T&>
        constexpr auto /*Point*/ operator/(const T& m) const;

        template<typename T>
        requires concepts::add_assign_as<R&, const T&>
        constexpr Point_<R, VSL_NAMESPACE::D1, CI>& /*Point*/ operator+=(const Vector_<T, VSL_NAMESPACE::D1, CI>& p);

        template<typename T>
        requires concepts::subtract_assign_as<R&, const T&>
        constexpr Point_<R, VSL_NAMESPACE::D1, CI>& /*Point*/ operator-=(const Vector_<T, VSL_NAMESPACE::D1, CI>& p);

        template<typename T>
        requires concepts::multiply_assign_as<R&, const T&>
        constexpr Point_<R, VSL_NAMESPACE::D1, CI>& /*Point*/ operator*=(const T& m);

        template<typename T>
        requires concepts::division_assign_as<R&, const T&>
        constexpr Point_<R, VSL_NAMESPACE::D1, CI>& /*Point*/ operator/=(const T& m);

        template<typename T>
        requires concepts::cast_as<R, T>
        constexpr Point_<T, VSL_NAMESPACE::D1, CI> cast() const;

        constexpr auto length() const;
    };

    template<typename R, typename CI>
    struct Point_<R, VSL_NAMESPACE::D2, CI> {
        using Dimension = typename VSL_NAMESPACE::D2;
        using ElementType = R;
        using CoordinateInfo = CI;

        static constexpr VSL_NAMESPACE::data_format::Format GRAPHIC_TYPE = VSL_NAMESPACE::data_format::convert_graphic_type<R>().template to_vec<(size_t)Dimension::DIMENSION>();

        coordinate::CoordinateSet<R, CI> value;

        constexpr Point_() {};

        constexpr Point_(R v1, R v2) : value(v1, v2) {};

        constexpr Point_(coordinate::CoordinateSet<R, CI> value) : value(value) {};

        constexpr bool operator<(const Point_<R, VSL_NAMESPACE::D2, CI>& p) const;

        constexpr auto operator<=>(const Point_<R, VSL_NAMESPACE::D2, CI>&) const = default;

        constexpr Point_<R, VSL_NAMESPACE::D2, CI> operator+() const;

        constexpr Point_<R, VSL_NAMESPACE::D2, CI> operator-() const;

        template<typename T>
        requires concepts::subtract_as<const R&, const T&>
        constexpr auto /*Vector*/ operator-(const Point_<T, VSL_NAMESPACE::D2, CI>& p) const;

        template<typename T>
        requires concepts::add_as<const R&, const T&>
        constexpr auto /*Point*/ operator+(const Vector_<T, VSL_NAMESPACE::D2, CI>& p) const;

        template<typename T>
        requires concepts::subtract_as<const R&, const T&>
        constexpr auto /*Point*/ operator-(const Vector_<T, VSL_NAMESPACE::D2, CI>& p) const;

        template<typename T>
        requires concepts::multiply_as<const R&, const T&>
        constexpr auto /*Point*/ operator*(const T& m) const;

        template<typename T>
        requires concepts::division_as<const R&, const T&>
        constexpr auto /*Point*/ operator/(const T& m) const;

        template<typename T>
        requires concepts::add_assign_as<R&, const T&>
        constexpr Point_<R, VSL_NAMESPACE::D2, CI>& /*Point*/ operator+=(const Vector_<T, VSL_NAMESPACE::D2, CI>& p);

        template<typename T>
        requires concepts::subtract_assign_as<R&, const T&>
        constexpr Point_<R, VSL_NAMESPACE::D2, CI>& /*Point*/ operator-=(const Vector_<T, VSL_NAMESPACE::D2, CI>& p);

        template<typename T>
        requires concepts::multiply_assign_as<R&, const T&>
        constexpr Point_<R, VSL_NAMESPACE::D2, CI>& /*Point*/ operator*=(const T& m);

        template<typename T>
        requires concepts::division_assign_as<R&, const T&>
        constexpr Point_<R, VSL_NAMESPACE::D2, CI>& /*Point*/ operator/=(const T& m);

        template<typename T>
        requires concepts::cast_as<R, T>
        constexpr Point_<T, VSL_NAMESPACE::D2, CI> cast() const;

        constexpr auto length() const;
    };

    template<typename R, typename CI>
    struct Point_<R, VSL_NAMESPACE::D3, CI> {
        using Dimension = typename VSL_NAMESPACE::D3;
        using ElementType = R;
        using CoordinateInfo = CI;

        static constexpr VSL_NAMESPACE::data_format::Format GRAPHIC_TYPE = VSL_NAMESPACE::data_format::convert_graphic_type<R>().template to_vec<(size_t)Dimension::DIMENSION>();

        coordinate::CoordinateSet<R, CI> value;

        constexpr Point_() {};

        constexpr Point_(R x, R y, R z) : value(x, y, z) {};

        constexpr Point_(coordinate::CoordinateSet<R, CI> value) : value(value) {};

        constexpr bool operator<(const Point_<R, VSL_NAMESPACE::D3, CI>& p) const;

        constexpr auto operator<=>(const Point_<R, VSL_NAMESPACE::D3, CI>&) const = default;

        constexpr Point_<R, VSL_NAMESPACE::D3, CI> operator+() const;

        constexpr Point_<R, VSL_NAMESPACE::D3, CI> operator-() const;

        template<typename T>
        requires concepts::subtract_as<const R&, const T&>
        constexpr auto /*Vector*/ operator-(const Point_<T, VSL_NAMESPACE::D3, CI>& p) const;

        template<typename T>
        requires concepts::add_as<const R&, const T&>
        constexpr auto /*Point*/ operator+(const Vector_<T, VSL_NAMESPACE::D3, CI>& p) const;

        template<typename T>
        requires concepts::subtract_as<const R&, const T&>
        constexpr auto /*Point*/ operator-(const Vector_<T, VSL_NAMESPACE::D3, CI>& p) const;

        template<typename T>
        requires concepts::multiply_as<const R&, const T&>
        constexpr auto /*Point*/ operator*(const T& m) const;

        template<typename T>
        requires concepts::division_as<const R&, const T&>
        constexpr auto /*Point*/ operator/(const T& m) const;

        template<typename T>
        requires concepts::add_assign_as<R&, const T&>
        constexpr Point_<R, VSL_NAMESPACE::D3, CI>& /*Point*/ operator+=(const Vector_<T, VSL_NAMESPACE::D3, CI>& p);

        template<typename T>
        requires concepts::subtract_assign_as<R&, const T&>
        constexpr Point_<R, VSL_NAMESPACE::D3, CI>& /*Point*/ operator-=(const Vector_<T, VSL_NAMESPACE::D3, CI>& p);

        template<typename T>
        requires concepts::multiply_assign_as<R&, const T&>
        constexpr Point_<R, VSL_NAMESPACE::D3, CI>& /*Point*/ operator*=(const T& m);

        template<typename T>
        requires concepts::division_assign_as<R&, const T&>
        constexpr Point_<R, VSL_NAMESPACE::D3, CI>& /*Point*/ operator/=(const T& m);

        template<typename T>
        requires concepts::cast_as<R, T>
        constexpr Point_<T, VSL_NAMESPACE::D3, CI> cast() const;

        constexpr auto length() const;
    };

    template<typename R, typename CI>
    Point_(coordinate::CoordinateSet<R, CI>)
        -> Point_<R, typename CI::Dimension, CI>;

    // ---------------------------------------------------------------------------------------------------------------------

    template<typename R, typename CI>
    constexpr bool Point_<R, VSL_NAMESPACE::D1, CI>::operator<(const Point_<R, VSL_NAMESPACE::D1, CI>& p) const {
        return length() < p.length();
    }

    template<typename R, typename CI>
    constexpr Point_<R, VSL_NAMESPACE::D1, CI> Point_<R, VSL_NAMESPACE::D1, CI>::operator+() const {
        return *this;
    }

    template<typename R, typename CI>
    constexpr Point_<R, VSL_NAMESPACE::D1, CI> Point_<R, VSL_NAMESPACE::D1, CI>::operator-() const {
        return Point_<R, VSL_NAMESPACE::D1, CI>(-value.___AN1);
    }

    template<typename R, typename CI>
    template<typename T>
    requires concepts::subtract_as<const R&, const T&>
    constexpr auto Point_<R, VSL_NAMESPACE::D1, CI>::operator-(const Point_<T, VSL_NAMESPACE::D1, CI>& p) const {
        return Vector_<decltype(value.___AN1 - p.value.___AN1), VSL_NAMESPACE::D1, CI>(value.___AN1 - p.value.___AN1);
    }

    template<typename R, typename CI>
    template<typename T>
    requires concepts::add_as<const R&, const T&>
    constexpr auto Point_<R, VSL_NAMESPACE::D1, CI>::operator+(const Vector_<T, VSL_NAMESPACE::D1, CI>& p) const {
        return Point_<decltype(value.___AN1 + p.value.___AN1), VSL_NAMESPACE::D1, CI>(value.___AN1 + p.value.___AN1);
    }

    template<typename R, typename CI>
    template<typename T>
    requires concepts::subtract_as<const R&, const T&>
    constexpr auto Point_<R, VSL_NAMESPACE::D1, CI>::operator-(const Vector_<T, VSL_NAMESPACE::D1, CI>& p) const {
        return Point_<decltype(value.___AN1 - p.value.___AN1), VSL_NAMESPACE::D1, CI>(value.___AN1 - p.value.___AN1);
    }

    template<typename R, typename CI>
    template<typename T>
    requires concepts::multiply_as<const R&, const T&>
    constexpr auto Point_<R, VSL_NAMESPACE::D1, CI>::operator*(const T& m) const {
        return Point_<decltype(value.___AN1 * m), VSL_NAMESPACE::D1, CI>(value.___AN1 * m);
    }

    template<typename R, typename CI>
    template<typename T>
    requires concepts::division_as<const R&, const T&>
    constexpr auto Point_<R, VSL_NAMESPACE::D1, CI>::operator/(const T& m) const {
        return Point_<decltype(value.___AN1 / m), VSL_NAMESPACE::D1, CI>(value.___AN1 / m);
    }

    template<typename R, typename CI>
    template<typename T>
    requires concepts::add_assign_as<R&, const T&>
    constexpr Point_<R, VSL_NAMESPACE::D1, CI>& Point_<R, VSL_NAMESPACE::D1, CI>::operator+=(const Vector_<T, VSL_NAMESPACE::D1, CI>& p) {
        this->value.___AN1 += p.value.___AN1;
        return *this;
    }

    template<typename R, typename CI>
    template<typename T>
    requires concepts::subtract_assign_as<R&, const T&>
    constexpr Point_<R, VSL_NAMESPACE::D1, CI>& Point_<R, VSL_NAMESPACE::D1, CI>::operator-=(const Vector_<T, VSL_NAMESPACE::D1, CI>& p) {
        this->value.___AN1 -= p.value.___AN1;
        return *this;
    }

    template<typename R, typename CI>
    template<typename T>
    requires concepts::multiply_assign_as<R&, const T&>
    constexpr Point_<R, VSL_NAMESPACE::D1, CI>& Point_<R, VSL_NAMESPACE::D1, CI>::operator*=(const T& m) {
        this->value.___AN1 *= m;
        return *this;
    }

    template<typename R, typename CI>
    template<typename T>
    requires concepts::division_assign_as<R&, const T&>
    constexpr Point_<R, VSL_NAMESPACE::D1, CI>& Point_<R, VSL_NAMESPACE::D1, CI>::operator/=(const T& m) {
        this->value.___AN1 /= m;
        return *this;
    }

    template<typename R, typename CI>
    template<typename T>
    requires concepts::cast_as<R, T>
    constexpr Point_<T, VSL_NAMESPACE::D1, CI> Point_<R, VSL_NAMESPACE::D1, CI>::cast() const {
        return Point_<T, VSL_NAMESPACE::D1, CI>((T)value.___AN1);
    }

    template<typename R, typename CI>
    constexpr auto Point_<R, VSL_NAMESPACE::D1, CI>::length() const {
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

    // ---------------------------------------------------------------------------------------------------------------------

    template<typename R, typename CI>
    constexpr bool Point_<R, VSL_NAMESPACE::D2, CI>::operator<(const Point_<R, VSL_NAMESPACE::D2, CI>& p) const {
        return value < p.value;
    }

    template<typename R, typename CI>
    constexpr Point_<R, VSL_NAMESPACE::D2, CI> Point_<R, VSL_NAMESPACE::D2, CI>::operator+() const {
        return *this;
    }

    template<typename R, typename CI>
    constexpr Point_<R, VSL_NAMESPACE::D2, CI> Point_<R, VSL_NAMESPACE::D2, CI>::operator-() const {
        return Point_<R, VSL_NAMESPACE::D2, CI>(-value.___AN1, -value.___AN2);
    }

    template<typename R, typename CI>
    template<typename T>
    requires concepts::subtract_as<const R&, const T&>
    constexpr auto Point_<R, VSL_NAMESPACE::D2, CI>::operator-(const Point_<T, VSL_NAMESPACE::D2, CI>& p) const {
        return Vector_<decltype(value.___AN1 - p.value.___AN1), VSL_NAMESPACE::D2, CI>(value.___AN1 - p.value.___AN1, value.___AN2 - p.value.___AN2);
    }

    template<typename R, typename CI>
    template<typename T>
    requires concepts::add_as<const R&, const T&>
    constexpr auto Point_<R, VSL_NAMESPACE::D2, CI>::operator+(const Vector_<T, VSL_NAMESPACE::D2, CI>& p) const {
        return Point_<decltype(value.___AN1 + p.value.___AN1), VSL_NAMESPACE::D2, CI>(value.___AN1 + p.value.___AN1, value.___AN2 + p.value.___AN2);
    }

    template<typename R, typename CI>
    template<typename T>
    requires concepts::subtract_as<const R&, const T&>
    constexpr auto Point_<R, VSL_NAMESPACE::D2, CI>::operator-(const Vector_<T, VSL_NAMESPACE::D2, CI>& p) const {
        return Point_<decltype(value.___AN1 - p.value.___AN1), VSL_NAMESPACE::D2, CI>(value.___AN1 - p.value.___AN1, value.___AN2 - p.value.___AN2);
    }

    template<typename R, typename CI>
    template<typename T>
    requires concepts::multiply_as<const R&, const T&>
    constexpr auto Point_<R, VSL_NAMESPACE::D2, CI>::operator*(const T& m) const {
        return Point_<decltype(value.___AN1 * m), VSL_NAMESPACE::D2, CI>(value.___AN1 * m, value.___AN2 * m);
    }

    template<typename R, typename CI>
    template<typename T>
    requires concepts::division_as<const R&, const T&>
    constexpr auto Point_<R, VSL_NAMESPACE::D2, CI>::operator/(const T& m) const {
        return Point_<decltype(value.___AN1 / m), VSL_NAMESPACE::D2, CI>(value.___AN1 / m, value.___AN2 / m);
    }

    template<typename R, typename CI>
    template<typename T>
    requires concepts::add_assign_as<R&, const T&>
    constexpr Point_<R, VSL_NAMESPACE::D2, CI>& Point_<R, VSL_NAMESPACE::D2, CI>::operator+=(const Vector_<T, VSL_NAMESPACE::D2, CI>& p) {
        this->value.___AN1 += p.value.___AN1;
        this->value.___AN2 += p.value.___AN2;
        return *this;
    }

    template<typename R, typename CI>
    template<typename T>
    requires concepts::subtract_assign_as<R&, const T&>
    constexpr Point_<R, VSL_NAMESPACE::D2, CI>& Point_<R, VSL_NAMESPACE::D2, CI>::operator-=(const Vector_<T, VSL_NAMESPACE::D2, CI>& p) {
        this->value.___AN1 -= p.value.___AN1;
        this->value.___AN2 -= p.value.___AN2;
        return *this;
    }

    template<typename R, typename CI>
    template<typename T>
    requires concepts::multiply_assign_as<R&, const T&>
    constexpr Point_<R, VSL_NAMESPACE::D2, CI>& Point_<R, VSL_NAMESPACE::D2, CI>::operator*=(const T& m) {
        this->value.___AN1 *= m;
        this->value.___AN2 *= m;
        return *this;
    }

    template<typename R, typename CI>
    template<typename T>
    requires concepts::division_assign_as<R&, const T&>
    constexpr Point_<R, VSL_NAMESPACE::D2, CI>& Point_<R, VSL_NAMESPACE::D2, CI>::operator/=(const T& m) {
        this->value.___AN1 /= m;
        this->value.___AN2 /= m;
        return *this;
    }

    template<typename R, typename CI>
    template<typename T>
    requires concepts::cast_as<R, T>
    constexpr Point_<T, VSL_NAMESPACE::D2, CI> Point_<R, VSL_NAMESPACE::D2, CI>::cast() const {
        return Point_<T, VSL_NAMESPACE::D2, CI>((T)value.___AN1, (T)value.___AN2);
    }

    template<typename R, typename CI>
    constexpr auto Point_<R, VSL_NAMESPACE::D2, CI>::length() const {
        return std::hypot(value.___AN1, value.___AN2);
    }

    // ---------------------------------------------------------------------------------------------------------------------

    template<typename R, typename CI>
    constexpr bool Point_<R, VSL_NAMESPACE::D3, CI>::operator<(const Point_<R, VSL_NAMESPACE::D3, CI>& p) const {
        return length() < p.length();
    }

    template<typename R, typename CI>
    constexpr Point_<R, VSL_NAMESPACE::D3, CI> Point_<R, VSL_NAMESPACE::D3, CI>::operator+() const {
        return *this;
    }

    template<typename R, typename CI>
    constexpr Point_<R, VSL_NAMESPACE::D3, CI> Point_<R, VSL_NAMESPACE::D3, CI>::operator-() const {
        return Point_<R, VSL_NAMESPACE::D3, CI>(-value.___AN1, -value.___AN2, -value.___AN3);
    }

    template<typename R, typename CI>
    template<typename T>
    requires concepts::subtract_as<const R&, const T&>
    constexpr auto Point_<R, VSL_NAMESPACE::D3, CI>::operator-(const Point_<T, VSL_NAMESPACE::D3, CI>& p) const {
        return Vector_<decltype(value.___AN1 - p.value.___AN1), VSL_NAMESPACE::D3, CI>(value.___AN1 - p.value.___AN1, value.___AN2 - p.value.___AN2, value.___AN3 - p.value.___AN3);
    }

    template<typename R, typename CI>
    template<typename T>
    requires concepts::add_as<const R&, const T&>
    constexpr auto Point_<R, VSL_NAMESPACE::D3, CI>::operator+(const Vector_<T, VSL_NAMESPACE::D3, CI>& p) const {
        return Point_<decltype(value.___AN1 + p.value.___AN1), VSL_NAMESPACE::D3, CI>(value.___AN1 + p.value.___AN1, value.___AN2 + p.value.___AN2, value.___AN3 + p.value.___AN3);
    }

    template<typename R, typename CI>
    template<typename T>
    requires concepts::subtract_as<const R&, const T&>
    constexpr auto Point_<R, VSL_NAMESPACE::D3, CI>::operator-(const Vector_<T, VSL_NAMESPACE::D3, CI>& p) const {
        return Point_<decltype(value.___AN1 - p.value.___AN1), VSL_NAMESPACE::D3, CI>(value.___AN1 - p.value.___AN1, value.___AN2 - p.value.___AN2, value.___AN3 - p.value.___AN3);
    }

    template<typename R, typename CI>
    template<typename T>
    requires concepts::multiply_as<const R&, const T&>
    constexpr auto Point_<R, VSL_NAMESPACE::D3, CI>::operator*(const T& m) const {
        return Point_<decltype(value.___AN1 * m), VSL_NAMESPACE::D3, CI>(value.___AN1 * m, value.___AN2 * m, value.___AN3 * m);
    }

    template<typename R, typename CI>
    template<typename T>
    requires concepts::division_as<const R&, const T&>
    constexpr auto Point_<R, VSL_NAMESPACE::D3, CI>::operator/(const T& m) const {
        return Point_<decltype(value.___AN1 / m), VSL_NAMESPACE::D3, CI>(value.___AN1 / m, value.___AN2 / m, value.___AN3 / m);
    }

    template<typename R, typename CI>
    template<typename T>
    requires concepts::add_assign_as<R&, const T&>
    constexpr Point_<R, VSL_NAMESPACE::D3, CI>& Point_<R, VSL_NAMESPACE::D3, CI>::operator+=(const Vector_<T, VSL_NAMESPACE::D3, CI>& p) {
        this->value.___AN1 += p.value.___AN1;
        this->value.___AN2 += p.value.___AN2;
        this->value.___AN3 += p.value.___AN3;
        return *this;
    }

    template<typename R, typename CI>
    template<typename T>
    requires concepts::subtract_assign_as<R&, const T&>
    constexpr Point_<R, VSL_NAMESPACE::D3, CI>& Point_<R, VSL_NAMESPACE::D3, CI>::operator-=(const Vector_<T, VSL_NAMESPACE::D3, CI>& p) {
        this->value.___AN1 -= p.value.___AN1;
        this->value.___AN2 -= p.value.___AN2;
        this->value.___AN3 -= p.value.___AN3;
        return *this;
    }

    template<typename R, typename CI>
    template<typename T>
    requires concepts::multiply_assign_as<R&, const T&>
    constexpr Point_<R, VSL_NAMESPACE::D3, CI>& Point_<R, VSL_NAMESPACE::D3, CI>::operator*=(const T& m) {
        this->value.___AN1 *= m;
        this->value.___AN2 *= m;
        this->value.___AN3 *= m;
        return *this;
    }

    template<typename R, typename CI>
    template<typename T>
    requires concepts::division_assign_as<R&, const T&>
    constexpr Point_<R, VSL_NAMESPACE::D3, CI>& Point_<R, VSL_NAMESPACE::D3, CI>::operator/=(const T& m) {
        this->value.___AN1 /= m;
        this->value.___AN2 /= m;
        this->value.___AN3 /= m;
        return *this;
    }

    template<typename R, typename CI>
    template<typename T>
    requires concepts::cast_as<R, T>
    constexpr Point_<T, VSL_NAMESPACE::D3, CI> Point_<R, VSL_NAMESPACE::D3, CI>::cast() const {
        return Point_<T, VSL_NAMESPACE::D3, CI>((T)value.___AN1, (T)value.___AN2, (T)value.___AN3);
    }

    template<typename R, typename CI>
    constexpr auto Point_<R, VSL_NAMESPACE::D3, CI>::length() const {
        return std::hypot(value.___AN1, value.___AN2, value.___AN3);
    }
} // namespace AHO_NAMESPACE
