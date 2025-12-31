#pragma once
#include "../define.hpp"
#include "../concepts.hpp"

#include "VSL/dimension.hpp"

#include <compare>
#include <cmath>
#include "ov.hpp"
#include "math/coordinate.hpp"
#include "math/mat.hpp"

namespace AHO_NAMESPACE {
	template<typename R, VSL_NAMESPACE::is_dimension D, typename CoordinateInfo>
	struct _Vector {
    };

	template<typename R = int, typename... Args>
		requires concepts::sames_as<R, Args...> && (!concepts::is_coordinate_set<R>)
	struct Vector : public _Vector<R, 
								   typename VSL_NAMESPACE::dimension<sizeof...(Args) + 1>::value,
								   typename AHO_NAMESPACE::coordinate::_DefaultCoordinateInfo<
								       typename VSL_NAMESPACE::dimension<sizeof...(Args) + 1>::value
								   >::value> {
		constexpr Vector() {};

		constexpr Vector(R x, Args... args) : _Vector<R,
											  typename VSL_NAMESPACE::dimension<sizeof...(Args) + 1>::value,
											  typename AHO_NAMESPACE::coordinate::_DefaultCoordinateInfo<
											      typename VSL_NAMESPACE::dimension<sizeof...(Args) + 1>::value
											  >::value>(x, args...) {};
	};

	namespace concepts {
		template<typename T>
		concept is_vector = requires{
			typename T::dimension;
			typename T::element_type;
            { std::convertible_to<T, _Vector<typename T::element_type, typename T::dimension>> };
		};
	}

	namespace d1 {
		template<typename R, typename CI>
		struct Vector : public AHO_NAMESPACE::_Vector<R, VSL_NAMESPACE::D1, CI> {
			constexpr Vector() {};

			constexpr Vector(R v) : AHO_NAMESPACE::_Vector<R, VSL_NAMESPACE::D1, CI>(v) {};
		};
		using VectorI = AHO_NAMESPACE::_Vector<int, VSL_NAMESPACE::D1>;
		using VectorF = AHO_NAMESPACE::_Vector<float, VSL_NAMESPACE::D1>;
		using VectorD = AHO_NAMESPACE::_Vector<double, VSL_NAMESPACE::D1>;
	}

	namespace d2 {
		template<typename R, typename CI>
		struct Vector : public AHO_NAMESPACE::_Vector<R, VSL_NAMESPACE::D2, CI> {
			constexpr Vector() {};

			constexpr Vector(R v1, R v2) : AHO_NAMESPACE::_Vector<R, VSL_NAMESPACE::D2, CI>(v1, v2) {};
		};
		using VectorI = AHO_NAMESPACE::_Vector<int, VSL_NAMESPACE::D2>;
		using VectorF = AHO_NAMESPACE::_Vector<float, VSL_NAMESPACE::D2>;
		using VectorD = AHO_NAMESPACE::_Vector<double, VSL_NAMESPACE::D2>;
	}

	namespace d3 {
		template<typename R, typename CI>
		struct Vector : public AHO_NAMESPACE::_Vector<R, VSL_NAMESPACE::D3, CI> {
			constexpr Vector() {};

			constexpr Vector(R x, R y, R z) : AHO_NAMESPACE::_Vector<R, VSL_NAMESPACE::D3, CI>(x, y, z) {};
		};
		using VectorI = AHO_NAMESPACE::_Vector<int, VSL_NAMESPACE::D3>;
		using VectorF = AHO_NAMESPACE::_Vector<float, VSL_NAMESPACE::D3>;
		using VectorD = AHO_NAMESPACE::_Vector<double, VSL_NAMESPACE::D3>;
	}

	template<typename R, typename CI>
	struct _Vector<R, VSL_NAMESPACE::D1, CI> {
		using dimension = typename VSL_NAMESPACE::D1;
		using element_type = R;
        using coordinate_info = CI;

		coordinate::_CoordinateSet<R, CI> value;

		constexpr _Vector() {};

		constexpr _Vector(R value) : value(value) {};

		constexpr _Vector(const coordinate::_CoordinateSet<R, CI>& value) : value(value) {};

		constexpr bool operator< (const _Vector<R, VSL_NAMESPACE::D1, CI>& p) const;

		constexpr auto operator <=>(const _Vector<R, VSL_NAMESPACE::D1, CI>&) const = default;

		constexpr _Vector<R, VSL_NAMESPACE::D1, CI> operator +() const;

		constexpr _Vector<R, VSL_NAMESPACE::D1, CI> operator -() const;

		template<concepts::add_as<R> T>
		constexpr auto operator +(const _Vector<T, VSL_NAMESPACE::D1, CI>& p) const;

		template<concepts::subtract_as<R> T>
		constexpr auto operator -(const _Vector<T, VSL_NAMESPACE::D1, CI>& p) const;

		template<concepts::multiply_as<R> T>
			requires (!concepts::is_vector<T>)
		constexpr auto operator *(const T& m) const;

		template<concepts::division_as<R> T>
		constexpr auto operator /(const T& m) const;

		template<concepts::add_as<R> T>
		constexpr _Vector<R, VSL_NAMESPACE::D1, CI>& operator +=(const _Vector<T, VSL_NAMESPACE::D1, CI>& p);

		template<concepts::subtract_as<R> T>
		constexpr _Vector<R, VSL_NAMESPACE::D1, CI>& operator -=(const _Vector<T, VSL_NAMESPACE::D1, CI>& p);

		template<concepts::multiply_as<R> T>
		constexpr _Vector<R, VSL_NAMESPACE::D1, CI>& operator *=(const T& m);

		template<concepts::division_as<R> T>
		constexpr _Vector<R, VSL_NAMESPACE::D1, CI>& operator /=(const T& m);

		template<concepts::cast_as<R> T>
		constexpr _Vector<T, VSL_NAMESPACE::D1, CI> cast() const;

        template<typename T>
        requires concepts::is_vector<T>&& std::convertible_to<_Vector<typename T::element_type, VSL_NAMESPACE::D1>, T>
        constexpr auto dot(const T& m) const;

        template<typename T>
        requires concepts::is_vector<T>&& std::convertible_to<_Vector<typename T::element_type, VSL_NAMESPACE::D1>, T>
        [[deprecated("cross() is undefined in 1D. Consider checking vector dimension.")]]
        constexpr auto cross(const T& m) const;

		constexpr auto length() const;

        constexpr _Vector<R, VSL_NAMESPACE::D1, CI>& normalize();

        constexpr auto get_normalized() const;

        constexpr operator Mat<R, 1, 1>() const;
	};

	template<typename R, typename CI>
	struct _Vector<R, VSL_NAMESPACE::D2, CI> {
		using dimension = typename VSL_NAMESPACE::D2;
		using element_type = R;
        using coordinate_info = CI;

		coordinate::_CoordinateSet<R, CI> value;

		constexpr _Vector() {};

		constexpr _Vector(R v1, R v2) : value(v1, v2) {};

		constexpr _Vector(const coordinate::_CoordinateSet<R, CI>& value) : value(value) {};

		constexpr bool operator< (const _Vector<R, VSL_NAMESPACE::D2, CI>& p) const;

		constexpr auto operator <=>(const _Vector<R, VSL_NAMESPACE::D2, CI>&) const = default;

		constexpr _Vector<R, VSL_NAMESPACE::D2, CI> operator +() const;

		constexpr _Vector<R, VSL_NAMESPACE::D2, CI> operator -() const;

		template<concepts::add_as<R> T>
		constexpr auto operator +(const _Vector<T, VSL_NAMESPACE::D2, CI>& p) const;

		template<concepts::subtract_as<R> T>
		constexpr auto operator -(const _Vector<T, VSL_NAMESPACE::D2, CI>& p) const;

		template<concepts::multiply_as<R> T>
			requires (!concepts::is_vector<T>)
		constexpr auto operator *(const T& m) const;

		template<concepts::division_as<R> T>
		constexpr auto operator /(const T& m) const;

		template<concepts::add_as<R> T>
		constexpr _Vector<T, VSL_NAMESPACE::D2, CI>& operator +=(const _Vector<T, VSL_NAMESPACE::D2, CI>& p);

		template<concepts::subtract_as<R> T>
		constexpr _Vector<T, VSL_NAMESPACE::D2, CI>& operator -=(const _Vector<T, VSL_NAMESPACE::D2, CI>& p);

		template<concepts::multiply_as<R> T>
		constexpr _Vector<T, VSL_NAMESPACE::D2, CI>& operator *=(const T& m);

		template<concepts::division_as<R> T>
		constexpr _Vector<T, VSL_NAMESPACE::D2, CI>& operator /=(const T& m);

		template<concepts::cast_as<R> T>
		constexpr _Vector<T, VSL_NAMESPACE::D2, CI> cast() const;

        template<typename T>
        requires concepts::is_vector<T>&& std::convertible_to<_Vector<typename T::element_type, VSL_NAMESPACE::D2>, T>
        constexpr auto dot(const T& m) const;

        template<typename T>
        requires concepts::is_vector<T>&& std::convertible_to<_Vector<typename T::element_type, VSL_NAMESPACE::D2>, T>
        constexpr auto cross(const T& m) const;

		constexpr auto length() const;

        constexpr _Vector<R, VSL_NAMESPACE::D2, CI>& normalize();

        constexpr auto get_normalized() const;

        constexpr operator Mat<R, 1, 2>() const;

        constexpr operator Mat<R, 2, 2>() const;
	};

	template<typename R, typename CI>
	struct _Vector<R, VSL_NAMESPACE::D3, CI> {
		using dimension = typename VSL_NAMESPACE::D3;
		using element_type = R;
        using coordinate_info = CI;

		coordinate::_CoordinateSet<R, CI> value;

		constexpr _Vector() {};

		constexpr _Vector(R x, R y, R z) : value(x, y, z) {};

		constexpr _Vector(const coordinate::_CoordinateSet<R, CI>& value) : value(value) {};

		constexpr bool operator< (const _Vector<R, VSL_NAMESPACE::D3, CI>& p) const;

		constexpr auto operator <=>(const _Vector<R, VSL_NAMESPACE::D3, CI>&) const = default;

		constexpr _Vector<R, VSL_NAMESPACE::D3, CI> operator +() const;

		constexpr _Vector<R, VSL_NAMESPACE::D3, CI> operator -() const;

		template<concepts::add_as<R> T>
		constexpr auto operator +(const _Vector<T, VSL_NAMESPACE::D3, CI>& p) const;

		template<concepts::subtract_as<R> T>
		constexpr auto operator -(const _Vector<T, VSL_NAMESPACE::D3, CI>& p) const;

		template<concepts::multiply_as<R> T>
			requires (!concepts::is_vector<T>)
		constexpr auto operator *(const T& m) const;

		template<concepts::division_as<R> T>
		constexpr auto operator /(const T& m) const;

		template<concepts::add_as<R> T>
		constexpr _Vector<T, VSL_NAMESPACE::D3, CI>& operator +=(const _Vector<T, VSL_NAMESPACE::D3, CI>& p);

		template<concepts::subtract_as<R> T>
		constexpr _Vector<T, VSL_NAMESPACE::D3, CI>& operator -=(const _Vector<T, VSL_NAMESPACE::D3, CI>& p);

		template<concepts::multiply_as<R> T>
		constexpr _Vector<T, VSL_NAMESPACE::D3, CI>& operator *=(const T& m);

		template<concepts::division_as<R> T>
		constexpr _Vector<T, VSL_NAMESPACE::D3, CI>& operator /=(const T& m);

		template<concepts::cast_as<R> T>
		constexpr _Vector<T, VSL_NAMESPACE::D3, CI> cast() const;

        template<typename T>
        requires concepts::is_vector<T>&& std::convertible_to<_Vector<typename T::element_type, VSL_NAMESPACE::D3, CI>, T>
        constexpr auto dot(const T& m) const;

        template<typename T>
        requires concepts::is_vector<T>&& std::convertible_to<_Vector<typename T::element_type, VSL_NAMESPACE::D3, CI>, T>
        constexpr auto cross(const T& m) const;

		constexpr auto length() const;

        constexpr _Vector<R, VSL_NAMESPACE::D3, CI>& normalize();

        constexpr auto get_normalized() const;

        constexpr operator Mat<R, 1, 4>() const;

        constexpr operator Mat<R, 4, 4>() const;
	};

    namespace product::dot {
        template<typename Dim, typename CI, typename R1, typename R2>
        constexpr auto operator *(_Vector<R1, Dim, CI> v1, _Vector<R2, Dim, CI> v2);
    }

    namespace product::cross {
        template<typename Dim, typename CI, typename R1, typename R2>
        constexpr auto operator *(_Vector<R1, Dim, CI> v1, _Vector<R2, Dim, CI> v2);
    }

    template<typename R, typename CI>
    _Vector(coordinate::_CoordinateSet<R, CI>)
    -> _Vector<R, typename CI::dimension, CI>;

    // ---------------------------------------------------------------------------------------------------------------------

    template<typename L, typename R, typename Dim, typename CI>
    constexpr auto operator *(const L& l, _Vector<R, Dim, CI> v1);

    // ---------------------------------------------------------------------------------------------------------------------

	template<typename R, typename CI>
	constexpr bool _Vector<R, VSL_NAMESPACE::D1, CI>::operator<(const _Vector<R, VSL_NAMESPACE::D1, CI>& p) const
	{
		return length() < p.length();
	}

	template<typename R, typename CI>
	constexpr _Vector<R, VSL_NAMESPACE::D1, CI> _Vector<R, VSL_NAMESPACE::D1, CI>::operator +() const {
		return *this;
	}

	template<typename R, typename CI>
	constexpr _Vector<R, VSL_NAMESPACE::D1, CI> _Vector<R, VSL_NAMESPACE::D1, CI>::operator -() const {
		return _Vector<R, VSL_NAMESPACE::D1, CI>(-value.___AN1);
	}

	template<typename R, typename CI>
	template<concepts::add_as<R> T>
	constexpr auto _Vector<R, VSL_NAMESPACE::D1, CI>::operator +(const _Vector<T, VSL_NAMESPACE::D1, CI>& p) const {
		return _Vector<decltype(std::declval<R>() + std::declval<T>()), VSL_NAMESPACE::D1, CI>(value.___AN1 + p.value.___AN1);
	}

	template<typename R, typename CI>
	template<concepts::subtract_as<R> T>
	constexpr auto _Vector<R, VSL_NAMESPACE::D1, CI>::operator -(const _Vector<T, VSL_NAMESPACE::D1, CI>& p) const {
		return _Vector<decltype(std::declval<R>() - std::declval<T>()), VSL_NAMESPACE::D1, CI>(value.___AN1 - p.value.___AN1);
	}

	template<typename R, typename CI>
	template<concepts::multiply_as<R> T>
		requires (!concepts::is_vector<T>)
	constexpr auto _Vector<R, VSL_NAMESPACE::D1, CI>::operator *(const T& m) const {
		return _Vector<decltype(std::declval<R>() * std::declval<T>()), VSL_NAMESPACE::D1, CI>(value.___AN1 * m);
	}

	template<typename R, typename CI>
	template<concepts::division_as<R> T>
	constexpr auto _Vector<R, VSL_NAMESPACE::D1, CI>::operator /(const T& m) const {
		return _Vector<decltype(std::declval<R>() / std::declval<T>()), VSL_NAMESPACE::D1, CI>(value.___AN1 / m);
	}

	template<typename R, typename CI>
	template<concepts::add_as<R> T>
	constexpr _Vector<R, VSL_NAMESPACE::D1, CI>& _Vector<R, VSL_NAMESPACE::D1, CI>::operator +=(const _Vector<T, VSL_NAMESPACE::D1, CI>& p) {
		this->value.___AN1 += p.value.___AN1;
		return *this;
	}

	template<typename R, typename CI>
	template<concepts::subtract_as<R> T>
	constexpr _Vector<R, VSL_NAMESPACE::D1, CI>& _Vector<R, VSL_NAMESPACE::D1, CI>::operator -=(const _Vector<T, VSL_NAMESPACE::D1, CI>& p) {
		this->value.___AN1 -= p.value.___AN1;
		return *this;
	}

	template<typename R, typename CI>
	template<concepts::multiply_as<R> T>
	constexpr _Vector<R, VSL_NAMESPACE::D1, CI>& _Vector<R, VSL_NAMESPACE::D1, CI>::operator *=(const T& p) {
		this->value.___AN1 *= p;
		return *this;
	}

	template<typename R, typename CI>
	template<concepts::division_as<R> T>
	constexpr _Vector<R, VSL_NAMESPACE::D1, CI>& _Vector<R, VSL_NAMESPACE::D1, CI>::operator /=(const T& p) {
		this->value.___AN1 /= p;
		return *this;
	}

	template<typename R, typename CI>
	template<concepts::cast_as<R> T>
	constexpr _Vector<T, VSL_NAMESPACE::D1, CI> _Vector<R, VSL_NAMESPACE::D1, CI>::cast() const
	{
		return _Vector<T, VSL_NAMESPACE::D1, CI>((T)value.___AN1);
	}

    template<typename R, typename CI>
    template<typename T>
        requires concepts::is_vector<T>&& std::convertible_to<_Vector<typename T::element_type, VSL_NAMESPACE::D1>, T>
    inline constexpr auto _Vector<R, VSL_NAMESPACE::D1, CI>::dot(const T& m) const
    {
        return (decltype(std::declval<R>() * std::declval<T>()))value.___AN1 * m.value.___AN1;
    }

    template<typename R, typename CI>
    template<typename T>
    requires concepts::is_vector<T>&& std::convertible_to<_Vector<typename T::element_type, VSL_NAMESPACE::D1>, T>
    inline constexpr auto _Vector<R, VSL_NAMESPACE::D1, CI>::cross(const T& m) const
    {
        return (decltype(std::declval<R>() * std::declval<T>()))0;
    }

    template<typename R, typename CI>
	constexpr auto _Vector<R, VSL_NAMESPACE::D1, CI>::length() const
	{
		return std::abs(value.___AN1);
	}

    template<typename R, typename CI>
    constexpr _Vector<R, VSL_NAMESPACE::D1, CI>& _Vector<R, VSL_NAMESPACE::D1, CI>::normalize()
    {
        this->value.___AN1 = 1;
        return *this;
    }

    template<typename R, typename CI>
    constexpr auto _Vector<R, VSL_NAMESPACE::D1, CI>::get_normalized() const
    {
        return _Vector<R, VSL_NAMESPACE::D1, CI>{ 1 };
    }

    template<typename R, typename CI>
    constexpr _Vector<R, VSL_NAMESPACE::D1, CI>::operator Mat<R, 1, 1>() const
    {
        return Mat<R, 1, 1>({ { value.___AN1} });
    }

	// ---------------------------------------------------------------------------------------------------------------------

	template<typename R, typename CI>
	constexpr bool _Vector<R, VSL_NAMESPACE::D2, CI>::operator<(const _Vector<R, VSL_NAMESPACE::D2, CI>& p) const
	{
		return length() < p.length();
	}

	template<typename R, typename CI>
	constexpr _Vector<R, VSL_NAMESPACE::D2, CI> _Vector<R, VSL_NAMESPACE::D2, CI>::operator +() const {
		return *this;
	}

	template<typename R, typename CI>
	constexpr _Vector<R, VSL_NAMESPACE::D2, CI> _Vector<R, VSL_NAMESPACE::D2, CI>::operator -() const {
		return _Vector<R, VSL_NAMESPACE::D2, CI>(-value.___AN1, -value.___AN2);
	}

	template<typename R, typename CI>
	template<concepts::add_as<R> T>
	constexpr auto _Vector<R, VSL_NAMESPACE::D2, CI>::operator +(const _Vector<T, VSL_NAMESPACE::D2, CI>& p) const {
		return _Vector<decltype(std::declval<R>() + std::declval<T>()), VSL_NAMESPACE::D2, CI>(value.___AN1 + p.value.___AN1, value.___AN2 + p.value.___AN2);
	}

	template<typename R, typename CI>
	template<concepts::subtract_as<R> T>
	constexpr auto _Vector<R, VSL_NAMESPACE::D2, CI>::operator -(const _Vector<T, VSL_NAMESPACE::D2, CI>& p) const {
		return _Vector<decltype(std::declval<R>() - std::declval<T>()), VSL_NAMESPACE::D2, CI>(value.___AN1 - p.value.___AN1, value.___AN2 - p.value.___AN2);
	}

	template<typename R, typename CI>
	template<concepts::multiply_as<R> T>
		requires (!concepts::is_vector<T>)
	constexpr auto _Vector<R, VSL_NAMESPACE::D2, CI>::operator *(const T& m) const {
		return _Vector<decltype(std::declval<R>() * std::declval<T>()), VSL_NAMESPACE::D2>(value.___AN1 * m, value.___AN2 * m);
	}

	template<typename R, typename CI>
	template<concepts::division_as<R> T>
	constexpr auto _Vector<R, VSL_NAMESPACE::D2, CI>::operator /(const T& m) const {
		return _Vector<decltype(std::declval<R>() / std::declval<T>()), VSL_NAMESPACE::D2>(value.___AN1 / m, value.___AN2 / m);
	}

	template<typename R, typename CI>
	template<concepts::add_as<R> T>
	constexpr _Vector<T, VSL_NAMESPACE::D2, CI>& _Vector<R, VSL_NAMESPACE::D2, CI>::operator +=(const _Vector<T, VSL_NAMESPACE::D2, CI>& p) {
		this->value.___AN1 += p.value.___AN1;
		this->value.___AN2 += p.value.___AN2;
		return *this;
	}

	template<typename R, typename CI>
	template<concepts::subtract_as<R> T>
	constexpr _Vector<T, VSL_NAMESPACE::D2, CI>& _Vector<R, VSL_NAMESPACE::D2, CI>::operator -=(const _Vector<T, VSL_NAMESPACE::D2, CI>& p) {
		this->value.___AN1 -= p.value.___AN1;
		this->value.___AN2 -= p.value.___AN2;
		return *this;
	}

	template<typename R, typename CI>
	template<concepts::multiply_as<R> T>
	constexpr _Vector<T, VSL_NAMESPACE::D2, CI>& _Vector<R, VSL_NAMESPACE::D2, CI>::operator *=(const T& p) {
		this->value.___AN1 *= p;
		this->value.___AN2 *= p;
		return *this;
	}

	template<typename R, typename CI>
	template<concepts::division_as<R> T>
	constexpr _Vector<T, VSL_NAMESPACE::D2, CI>& _Vector<R, VSL_NAMESPACE::D2, CI>::operator /=(const T& p) {
		this->value.___AN1 /= p;
		this->value.___AN2 /= p;
		return *this;
	}

	template<typename R, typename CI>
	template<concepts::cast_as<R> T>
	constexpr _Vector<T, VSL_NAMESPACE::D2, CI> _Vector<R, VSL_NAMESPACE::D2, CI>::cast() const
	{
		return _Vector<T, VSL_NAMESPACE::D2, CI>((T)value.___AN1, (T)value.___AN2);
	}

    template<typename R, typename CI>
    template<typename T>
        requires concepts::is_vector<T>&& std::convertible_to<_Vector<typename T::element_type, VSL_NAMESPACE::D2>, T>
    constexpr auto _Vector<R, VSL_NAMESPACE::D2, CI>::dot(const T& m) const {
        return value.___AN1 * m.value.___AN1 + value.___AN2 * m.value.___AN2;
    }

    template<typename R, typename CI>
    template<typename T>
        requires concepts::is_vector<T>&& std::convertible_to<_Vector<typename T::element_type, VSL_NAMESPACE::D2>, T>
    constexpr auto _Vector<R, VSL_NAMESPACE::D2, CI>::cross(const T& m) const {
        return _Vector<decltype(value.___AN1 * m.value.___AN2),
                VSL_NAMESPACE::D1,
                typename coordinate::_MakeCoordinateInfo<(!CI::EnabledX), (!CI::EnabledY), (!CI::EnabledZ)>::value>
                (value.___AN1 * m.value.___AN2 - value.___AN2 * m.value.___AN1);
    }

	template<typename R, typename CI>
	constexpr auto _Vector<R, VSL_NAMESPACE::D2, CI>::length() const
	{
		return std::sqrt(std::abs(value.___AN1 * value.___AN1 + value.___AN2 * value.___AN2));
	}

    template<typename R, typename CI>
    constexpr _Vector<R, VSL_NAMESPACE::D2, CI>& _Vector<R, VSL_NAMESPACE::D2, CI>::normalize()
    {
        *this /= length();
        return *this;
    }

    template<typename R, typename CI>
    constexpr auto _Vector<R, VSL_NAMESPACE::D2, CI>::get_normalized() const
    {
        return *this / length();
    }

    template<typename R, typename CI>
    constexpr _Vector<R, VSL_NAMESPACE::D2, CI>::operator Mat<R, 1, 2>() const
    {
        return Mat<R, 1, 2>({ { value.___AN1, value.___AN2 } });
    }

    template<typename R, typename CI>
    constexpr _Vector<R, VSL_NAMESPACE::D2, CI>::operator Mat<R, 2, 2>() const
    {
        return Mat<R, 2, 2>({ { value.___AN1, 0 }, { 0, value.___AN2 } });
    }

	// ---------------------------------------------------------------------------------------------------------------------

	template<typename R, typename CI>
	constexpr bool _Vector<R, VSL_NAMESPACE::D3, CI>::operator<(const _Vector<R, VSL_NAMESPACE::D3, CI>& p) const
	{
		return length() < p.length();
	}

	template<typename R, typename CI>
	constexpr _Vector<R, VSL_NAMESPACE::D3, CI> _Vector<R, VSL_NAMESPACE::D3, CI>::operator +() const {
		return *this;
	}

	template<typename R, typename CI>
	constexpr _Vector<R, VSL_NAMESPACE::D3, CI> _Vector<R, VSL_NAMESPACE::D3, CI>::operator -() const {
		return _Vector<R, VSL_NAMESPACE::D3, CI>(-value.___AN1, -value.___AN2, -value.___AN3);
	}

	template<typename R, typename CI>
	template<concepts::add_as<R> T>
	constexpr auto _Vector<R, VSL_NAMESPACE::D3, CI>::operator +(const _Vector<T, VSL_NAMESPACE::D3, CI>& p) const {
		return _Vector<decltype(std::declval<R>() + std::declval<T>()), VSL_NAMESPACE::D3, CI>(value.___AN1 + p.value.___AN1, value.___AN2 + p.value.___AN2, value.___AN3 + p.value.___AN3);
	}

	template<typename R, typename CI>
	template<concepts::subtract_as<R> T>
	constexpr auto _Vector<R, VSL_NAMESPACE::D3, CI>::operator -(const _Vector<T, VSL_NAMESPACE::D3, CI>& p) const {
		return _Vector<decltype(std::declval<R>() - std::declval<T>()), VSL_NAMESPACE::D3, CI>(value.___AN1 - p.value.___AN1, value.___AN2 - p.value.___AN2, value.___AN3 - p.value.___AN3);
	}

	template<typename R, typename CI>
	template<concepts::multiply_as<R> T>


		requires (!concepts::is_vector<T>)
	constexpr auto _Vector<R, VSL_NAMESPACE::D3, CI>::operator *(const T& m) const {
		return _Vector<decltype(std::declval<R>() * std::declval<T>()), VSL_NAMESPACE::D3, CI>(value.___AN1 * m, value.___AN2 * m, value.___AN3 * m);
	}

	template<typename R, typename CI>
	template<concepts::division_as<R> T>
	constexpr auto _Vector<R, VSL_NAMESPACE::D3, CI>::operator /(const T& m) const {
		return _Vector<decltype(std::declval<R>() / std::declval<T>()), VSL_NAMESPACE::D3>(value.___AN1 / m, value.___AN2 / m, value.___AN3 / m);
	}

	template<typename R, typename CI>
	template<concepts::add_as<R> T>
	constexpr _Vector<T, VSL_NAMESPACE::D3, CI>& _Vector<R, VSL_NAMESPACE::D3, CI>::operator +=(const _Vector<T, VSL_NAMESPACE::D3, CI>& p) {
		this->value.___AN1 += p.value.___AN1;
		this->value.___AN2 += p.value.___AN2;
		this->value.___AN3 += p.value.___AN3;
		return *this;
	}

	template<typename R, typename CI>
	template<concepts::subtract_as<R> T>
	constexpr _Vector<T, VSL_NAMESPACE::D3, CI>& _Vector<R, VSL_NAMESPACE::D3, CI>::operator -=(const _Vector<T, VSL_NAMESPACE::D3, CI>& p) {
		this->value.___AN1 -= p.value.___AN1;
		this->value.___AN2 -= p.value.___AN2;
		this->value.___AN3 -= p.value.___AN3;
		return *this;
	}

	template<typename R, typename CI>
	template<concepts::multiply_as<R> T>
	constexpr _Vector<T, VSL_NAMESPACE::D3, CI>& _Vector<R, VSL_NAMESPACE::D3, CI>::operator *=(const T& p) {
		this->value.___AN1 *= p;
		this->value.___AN2 *= p;
		this->value.___AN3 *= p;
		return *this;
	}

	template<typename R, typename CI>
	template<concepts::division_as<R> T>
	constexpr _Vector<T, VSL_NAMESPACE::D3, CI>& _Vector<R, VSL_NAMESPACE::D3, CI>::operator /=(const T& p) {
		this->value.___AN1 /= p;
		this->value.___AN2 /= p;
		this->value.___AN3 /= p;
		return *this;
	}

	template<typename R, typename CI>
	template<concepts::cast_as<R> T>
	constexpr _Vector<T, VSL_NAMESPACE::D3, CI> _Vector<R, VSL_NAMESPACE::D3, CI>::cast() const
	{
		return _Vector<T, VSL_NAMESPACE::D3, CI>((T)value.___AN1, (T)value.___AN2, (T)value.___AN3);
	}

    template<typename R, typename CI>
    template<typename T>
    requires concepts::is_vector<T>&& std::convertible_to<_Vector<typename T::element_type, VSL_NAMESPACE::D3, CI>, T>
    inline constexpr auto _Vector<R, VSL_NAMESPACE::D3, CI>::dot(const T& m) const
    {
        return value.___AN1 * m.value.___AN1 + value.___AN2 * m.value.___AN2 + value.___AN3 * m.value.___AN3;
    }

    template<typename R, typename CI>
    template<typename T>
    requires concepts::is_vector<T>&& std::convertible_to<_Vector<typename T::element_type, VSL_NAMESPACE::D3, CI>, T>
    inline constexpr auto _Vector<R, VSL_NAMESPACE::D3, CI>::cross(const T& m) const
    {
        return _Vector<R, VSL_NAMESPACE::D3, CI>{
                value.y.value * m.value.z.value - value.z.value * m.value.y.value,
                value.z.value * m.value.x.value - value.x.value * m.value.z.value,
                value.x.value * m.value.y.value - value.y.value * m.value.x.value
        };
    }

	template<typename R, typename CI>
	constexpr auto _Vector<R, VSL_NAMESPACE::D3, CI>::length() const
	{
		return std::sqrt(std::abs(value.___AN1 * value.___AN1 + value.___AN2 * value.___AN2 + value.___AN3 * value.___AN3));
	}

    template<typename R, typename CI>
    constexpr _Vector<R, VSL_NAMESPACE::D3, CI>& _Vector<R, VSL_NAMESPACE::D3, CI>::normalize()
    {
        *this /= length();
        return *this;
    }

    template<typename R, typename CI>
    constexpr auto _Vector<R, VSL_NAMESPACE::D3, CI>::get_normalized() const
    {
        return *this / length();
    }

    template<typename R, typename CI>
    constexpr _Vector<R, VSL_NAMESPACE::D3, CI>::operator Mat<R, 1, 4>() const
    {
        return Mat<R, 1, 4>({ { value.___AN1, value.___AN2, value.___AN3, 0 } });
    }

    template<typename R, typename CI>
    constexpr _Vector<R, VSL_NAMESPACE::D3, CI>::operator Mat<R, 4, 4>() const
    {
        return Mat<R, 4, 4>({ { value.___AN1, 0, 0, 0 }, { 0, value.___AN2, 0, 0 }, { 0, 0, value.___AN3, 0 } });
    }

    // ---------------------------------------------------------------------------------------------------------------------

    namespace product::dot {
        template<typename Dim, typename CI, typename R1, typename R2>
        constexpr auto operator *(_Vector<R1, Dim, CI> v1, _Vector<R2, Dim, CI> v2) {
            return v1.dot(v2);
        }
    }

    namespace product::cross {
        template<typename Dim, typename CI, typename R1, typename R2>
        constexpr auto operator *(_Vector<R1, Dim, CI> v1, _Vector<R2, Dim, CI> v2) {
            return v1.cross(v2);
        }
    }

    // ---------------------------------------------------------------------------------------------------------------------

    template<typename L, typename R, typename Dim, typename CI>
    constexpr auto operator *(const L& l, _Vector<R, Dim, CI> v1) {
        return v1 * l;
    }
}