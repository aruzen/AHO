#pragma once
#include "../define.h"
#include "../concepts.h"

#include <VSL/dimention.h>
#include <VSL/format.h>

#include "math/coordinate.h"

#include "Vector.h"

namespace AHO_NAMESPACE {
	template<typename R = int, 
		VSL_NAMESPACE::is_dimention D = VSL_NAMESPACE::VSL_DEFAULT_DIMENTION_STRUCT,
		typename CoordinateInfo = AHO_NAMESPACE::coordinate::_DefaultCoordinateInfo<D>::value>
	struct _Point {
	};

	namespace concepts {
		template<typename T>
		concept is_point = requires(T t) {
			typename std::remove_reference_t<T>::dimention;
			typename std::remove_reference_t<T>::element_type;
			std::convertible_to<std::remove_reference_t<T>, _Point<typename std::remove_reference_t<T>::element_type, typename std::remove_reference_t<T>::dimention>>;
			(_Point<typename std::remove_reference_t<T>::element_type, typename std::remove_reference_t<T>::dimention>)t;
		};
	}

	template<typename R = int, typename... Args>
		requires concepts::sames_as<R, Args...>
	struct Point : public _Point<R, 
								 typename VSL_NAMESPACE::Dimention<sizeof...(Args) + 1>::value, 
								 typename AHO_NAMESPACE::coordinate::_DefaultCoordinateInfo<
								     typename VSL_NAMESPACE::Dimention<sizeof...(Args) + 1>::value
								 >::value> {
		constexpr Point() {};

		constexpr Point(R x, Args... args) : _Point<R, typename VSL_NAMESPACE::Dimention<sizeof...(Args) + 1>::value>(x, args...) {};
	};

	namespace d1 {
		template<typename R, typename CI = AHO_NAMESPACE::coordinate::_DefaultCoordinateInfo<VSL_NAMESPACE::D1>>
		struct Point : public AHO_NAMESPACE::_Point<R, VSL_NAMESPACE::D1, CI> {
			constexpr Point() {};
			
			constexpr Point(R x) : AHO_NAMESPACE::_Point<R, VSL_NAMESPACE::D1, CI>(x) {};
		};
		using PointI = AHO_NAMESPACE::_Point<int, VSL_NAMESPACE::D1>;
		using PointF = AHO_NAMESPACE::_Point<float, VSL_NAMESPACE::D1>;
		using PointD = AHO_NAMESPACE::_Point<double, VSL_NAMESPACE::D1>;
	}

	namespace d2 {
		template<typename R, typename CI = AHO_NAMESPACE::coordinate::_DefaultCoordinateInfo<VSL_NAMESPACE::D2>>
		struct Point : public AHO_NAMESPACE::_Point<R, VSL_NAMESPACE::D2, CI> {
			constexpr Point() {};
			
			constexpr Point(R x, R y) : AHO_NAMESPACE::_Point<R, VSL_NAMESPACE::D2, CI>(x, y) {};
		};
		using PointI = AHO_NAMESPACE::_Point<int, VSL_NAMESPACE::D2>;
		using PointF = AHO_NAMESPACE::_Point<float, VSL_NAMESPACE::D2>;
		using PointD = AHO_NAMESPACE::_Point<double, VSL_NAMESPACE::D2>;
	}

	namespace d3 {
		template<typename R, typename CI = AHO_NAMESPACE::coordinate::_DefaultCoordinateInfo<VSL_NAMESPACE::D3>>
		struct Point : public AHO_NAMESPACE::_Point<R, VSL_NAMESPACE::D3, CI> {
			constexpr Point() {};
			
			constexpr Point(R x, R y, R z) : AHO_NAMESPACE::_Point<R, VSL_NAMESPACE::D3, CI>(x, y, z) {};
		};
		using PointI = AHO_NAMESPACE::_Point<int, VSL_NAMESPACE::D3>;
		using PointF = AHO_NAMESPACE::_Point<float, VSL_NAMESPACE::D3>;
		using PointD = AHO_NAMESPACE::_Point<double, VSL_NAMESPACE::D3>;
	}

	template<typename R, typename CI>
	struct _Point<R, VSL_NAMESPACE::D1, CI> {
		using dimention = typename VSL_NAMESPACE::D1;
		using element_type = R;

        constexpr static VSL_NAMESPACE::data_format::___Format graphic_type = VSL_NAMESPACE::data_format::convert_graphic_type<R>()
                .template toVec<(size_t)dimention::dimention>();

		coordinate::_CoordinateSet<R, CI> value;

		constexpr _Point() {};

		constexpr _Point(R value) : value(value) {};

		constexpr _Point(coordinate::_CoordinateSet<R, CI> value) : value(value) {};

		constexpr bool operator< (const _Point<R, VSL_NAMESPACE::D1, CI>& p) const;

		constexpr auto operator <=>(const _Point<R, VSL_NAMESPACE::D1, CI>&) const = default;

		constexpr _Point<R, VSL_NAMESPACE::D1, CI> operator +() const;

		constexpr _Point<R, VSL_NAMESPACE::D1, CI> operator -() const;

		template<concepts::subtract_as<R> T>
		constexpr auto /*Vector*/ operator -(const _Point<T, VSL_NAMESPACE::D1, CI>& p) const;

		template<concepts::add_as<R> T>
		constexpr auto /*Point*/ operator +(const _Vector<T, VSL_NAMESPACE::D1, CI>& p) const;

		template<concepts::subtract_as<R> T>
		constexpr auto /*Point*/ operator -(const _Vector<T, VSL_NAMESPACE::D1, CI>& p) const;

		template<concepts::multiply_as<R> T>
		constexpr auto /*Point*/ operator *(const T& m) const;

		template<concepts::division_as<R> T>
		constexpr auto /*Point*/ operator /(const T& m) const;

		template<concepts::add_as<R> T>
		constexpr _Point<R, VSL_NAMESPACE::D1, CI>& /*Point*/ operator +=(const _Vector<T, VSL_NAMESPACE::D1, CI>& p);

		template<concepts::subtract_as<R> T>
		constexpr _Point<R, VSL_NAMESPACE::D1, CI>& /*Point*/ operator -=(const _Vector<T, VSL_NAMESPACE::D1, CI>& p);

		template<concepts::multiply_as<R> T>
		constexpr _Point<R, VSL_NAMESPACE::D1, CI>& /*Point*/ operator *=(const T& m);

		template<concepts::division_as<R> T>
		constexpr _Point<R, VSL_NAMESPACE::D1, CI>& /*Point*/ operator /=(const T& m);

		template<concepts::cast_as<R> T>
		constexpr _Point<T, VSL_NAMESPACE::D1, CI> cast() const;

		constexpr auto length() const;
	};

	template<typename R, typename CI>
	struct _Point<R, VSL_NAMESPACE::D2, CI> {
		using dimention = typename VSL_NAMESPACE::D2;
		using element_type = R;
		
		static constexpr VSL_NAMESPACE::data_format::___Format graphic_type
			= VSL_NAMESPACE::data_format::convert_graphic_type<R>().template toVec<(size_t)dimention::dimention>();

		coordinate::_CoordinateSet<R, CI> value;

		constexpr _Point() {};

		constexpr _Point(R v1, R v2) : value(v1, v2) {};

		constexpr _Point(coordinate::_CoordinateSet<R, CI> value) : value(value) {};

		constexpr bool operator< (const _Point<R, VSL_NAMESPACE::D2, CI>& p) const;

		constexpr auto operator <=>(const _Point<R, VSL_NAMESPACE::D2, CI>&) const = default;

		constexpr _Point<R, VSL_NAMESPACE::D2, CI> operator +() const;

		constexpr _Point<R, VSL_NAMESPACE::D2, CI> operator -() const;

		template<concepts::subtract_as<R> T>
		constexpr auto /*Vector*/ operator -(const _Point<T, VSL_NAMESPACE::D2, CI>& p) const;

		template<concepts::add_as<R> T>
		constexpr auto /*Point*/ operator +(const _Vector<T, VSL_NAMESPACE::D2, CI>& p) const;

		template<concepts::subtract_as<R> T>
		constexpr auto /*Point*/ operator -(const _Vector<T, VSL_NAMESPACE::D2, CI>& p) const;

		template<concepts::multiply_as<R> T>
		constexpr auto /*Point*/ operator *(const T& m) const;

		template<concepts::division_as<R> T>
		constexpr auto /*Point*/ operator /(const T& m) const;

		template<concepts::add_as<R> T>
		constexpr _Point<R, VSL_NAMESPACE::D2, CI>& /*Point*/ operator +=(const _Vector<T, VSL_NAMESPACE::D2, CI>& p);

		template<concepts::subtract_as<R> T>
		constexpr _Point<R, VSL_NAMESPACE::D2, CI>& /*Point*/ operator -=(const _Vector<T, VSL_NAMESPACE::D2, CI>& p);

		template<concepts::multiply_as<R> T>
		constexpr _Point<R, VSL_NAMESPACE::D2, CI>& /*Point*/ operator *=(const T& m);

		template<concepts::division_as<R> T>
		constexpr _Point<R, VSL_NAMESPACE::D2, CI>& /*Point*/ operator /=(const T& m);

		template<concepts::cast_as<R> T>
		constexpr _Point<T, VSL_NAMESPACE::D2, CI> cast() const;

		constexpr auto length() const;
	};

	template<typename R, typename CI>
	struct _Point<R, VSL_NAMESPACE::D3, CI> {
		using dimention = typename VSL_NAMESPACE::D3;
		using element_type = R;

		static constexpr VSL_NAMESPACE::data_format::___Format graphic_type
			= VSL_NAMESPACE::data_format::convert_graphic_type<R>().template toVec<(size_t)dimention::dimention>();

		coordinate::_CoordinateSet<R, CI> value;

		constexpr _Point() {};

		constexpr _Point(R x, R y, R z) : value(x, y, z) {};

		constexpr _Point(coordinate::_CoordinateSet<R, CI> value) : value(value) {};

		constexpr bool operator< (const _Point<R, VSL_NAMESPACE::D3, CI>& p) const;

		constexpr auto operator <=>(const _Point<R, VSL_NAMESPACE::D3, CI>&) const = default;

		constexpr _Point<R, VSL_NAMESPACE::D3, CI> operator +() const;

		constexpr _Point<R, VSL_NAMESPACE::D3, CI> operator -() const;

		template<concepts::subtract_as<R> T>
		constexpr auto /*Vector*/ operator -(const _Point<T, VSL_NAMESPACE::D3, CI>& p) const;

		template<concepts::add_as<R> T>
		constexpr auto /*Point*/ operator +(const _Vector<T, VSL_NAMESPACE::D3, CI>& p) const;

		template<concepts::subtract_as<R> T>
		constexpr auto /*Point*/ operator -(const _Vector<T, VSL_NAMESPACE::D3, CI>& p) const;

		template<concepts::multiply_as<R> T>
		constexpr auto /*Point*/ operator *(const T& m) const;

		template<concepts::division_as<R> T>
		constexpr auto /*Point*/ operator /(const T& m) const;

		template<concepts::add_as<R> T>
		constexpr _Point<R, VSL_NAMESPACE::D3, CI>& /*Point*/ operator +=(const _Vector<T, VSL_NAMESPACE::D3, CI>& p);

		template<concepts::subtract_as<R> T>
		constexpr _Point<R, VSL_NAMESPACE::D3, CI>& /*Point*/ operator -=(const _Vector<T, VSL_NAMESPACE::D3, CI>& p);

		template<concepts::multiply_as<R> T>
		constexpr _Point<R, VSL_NAMESPACE::D3, CI>& /*Point*/ operator *=(const T& m);

		template<concepts::division_as<R> T>
		constexpr _Point<R, VSL_NAMESPACE::D3, CI>& /*Point*/ operator /=(const T& m);

		template<concepts::cast_as<R> T>
		constexpr _Point<T, VSL_NAMESPACE::D3, CI> cast() const;

		constexpr auto length() const;
	};

	// ---------------------------------------------------------------------------------------------------------------------

	template<typename R, typename CI>
	constexpr bool _Point<R, VSL_NAMESPACE::D1, CI>::operator<(const _Point<R, VSL_NAMESPACE::D1, CI>& p) const
	{
		return length() < p.length();
	}

	template<typename R, typename CI>
	constexpr _Point<R, VSL_NAMESPACE::D1, CI> _Point<R, VSL_NAMESPACE::D1, CI>::operator +() const {
		return *this;
	}

	template<typename R, typename CI>
	constexpr _Point<R, VSL_NAMESPACE::D1, CI> _Point<R, VSL_NAMESPACE::D1, CI>::operator -() const {
		return _Point<R, VSL_NAMESPACE::D1, CI>(-value.___AN1);
	}

	template<typename R, typename CI>
	template<concepts::subtract_as<R> T>
	constexpr auto _Point<R, VSL_NAMESPACE::D1, CI>::operator -(const _Point<T, VSL_NAMESPACE::D1, CI>& p) const {
		return _Vector<decltype(std::declval<R>() - std::declval<T>()), VSL_NAMESPACE::D1, CI>(value.___AN1 - p.value.___AN1);
	}

	template<typename R, typename CI>
	template<concepts::add_as<R> T>
	constexpr auto _Point<R, VSL_NAMESPACE::D1, CI>::operator +(const _Vector<T, VSL_NAMESPACE::D1, CI>& p) const {
		return _Point<decltype(std::declval<R>() + std::declval<T>()), VSL_NAMESPACE::D1, CI>(value.___AN1 + p.value.___AN1);
	}

	template<typename R, typename CI>
	template<concepts::subtract_as<R> T>
	constexpr auto _Point<R, VSL_NAMESPACE::D1, CI>::operator -(const _Vector<T, VSL_NAMESPACE::D1, CI>& p) const {
		return _Point<decltype(std::declval<R>() - std::declval<T>()), VSL_NAMESPACE::D1, CI>(value.___AN1 - p.value.___AN1);
	}

	template<typename R, typename CI>
	template<concepts::multiply_as<R> T>
	constexpr auto _Point<R, VSL_NAMESPACE::D1, CI>::operator *(const T& m) const {
		return _Point<decltype(std::declval<R>()* m), VSL_NAMESPACE::D1, CI>(value.___AN1 * m);
	}

	template<typename R, typename CI>
	template<concepts::division_as<R> T>
	constexpr auto _Point<R, VSL_NAMESPACE::D1, CI>::operator /(const T& m) const {
		return _Point<decltype(std::declval<R>() / m), VSL_NAMESPACE::D1, CI>(value.___AN1 / m);
	}

	template<typename R, typename CI>
	template<concepts::add_as<R> T>
	constexpr _Point<R, VSL_NAMESPACE::D1, CI>& _Point<R, VSL_NAMESPACE::D1, CI>::operator +=(const _Vector<T, VSL_NAMESPACE::D1, CI>& p) {
		this->value.___AN1 += p.value.___AN1;
		return *this;
	}

	template<typename R, typename CI>
	template<concepts::subtract_as<R> T>
	constexpr _Point<R, VSL_NAMESPACE::D1, CI>& _Point<R, VSL_NAMESPACE::D1, CI>::operator -=(const _Vector<T, VSL_NAMESPACE::D1, CI>& p) {
		this->value.___AN1 -= p.value.___AN1; 
		return *this;
	}

	template<typename R, typename CI>
	template<concepts::multiply_as<R> T>
	constexpr _Point<R, VSL_NAMESPACE::D1, CI>& _Point<R, VSL_NAMESPACE::D1, CI>::operator *=(const T& m) {
		this->value.___AN1 *= m; 
		return *this;
	}

	template<typename R, typename CI>
	template<concepts::division_as<R> T>
	constexpr _Point<R, VSL_NAMESPACE::D1, CI>& _Point<R, VSL_NAMESPACE::D1, CI>::operator /=(const T& m) {
		this->value.___AN1 /= m; 
		return *this;
	}

	template<typename R, typename CI>
	template<concepts::cast_as<R> T>
	constexpr _Point<T, VSL_NAMESPACE::D1, CI> _Point<R, VSL_NAMESPACE::D1, CI>::cast() const
	{
		return _Point<T, VSL_NAMESPACE::D1, CI>((T)value.___AN1);
	}

	template<typename R, typename CI>
	constexpr auto _Point<R, VSL_NAMESPACE::D1, CI>::length() const
	{
		return value.___AN1;
	}

	// ---------------------------------------------------------------------------------------------------------------------

	template<typename R, typename CI>
	constexpr bool _Point<R, VSL_NAMESPACE::D2, CI>::operator<(const _Point<R, VSL_NAMESPACE::D2, CI>& p) const
	{
		return length() < p.length();
	}

	template<typename R, typename CI>
	constexpr _Point<R, VSL_NAMESPACE::D2, CI> _Point<R, VSL_NAMESPACE::D2, CI>::operator +() const {
		return *this;
	}

	template<typename R, typename CI>
	constexpr _Point<R, VSL_NAMESPACE::D2, CI> _Point<R, VSL_NAMESPACE::D2, CI>::operator -() const {
		return _Point<R, VSL_NAMESPACE::D2, CI>(-value.___AN1, -value.___AN2);
	}

	template<typename R, typename CI>
	template<concepts::subtract_as<R> T>
	constexpr auto _Point<R, VSL_NAMESPACE::D2, CI>::operator -(const _Point<T, VSL_NAMESPACE::D2, CI>& p) const {
		return _Vector<decltype(std::declval<R>() - std::declval<T>()), VSL_NAMESPACE::D2, CI>(value.___AN1 - p.value.___AN1, value.___AN2 - p.value.___AN2);
	}

	template<typename R, typename CI>
	template<concepts::add_as<R> T>
	constexpr auto _Point<R, VSL_NAMESPACE::D2, CI>::operator +(const _Vector<T, VSL_NAMESPACE::D2, CI>& p) const {
		return _Point<decltype(std::declval<R>() + std::declval<T>()), VSL_NAMESPACE::D2, CI>(value.___AN1 + p.value.___AN1, value.___AN2 + p.value.___AN2);
	}

	template<typename R, typename CI>
	template<concepts::subtract_as<R> T>
	constexpr auto _Point<R, VSL_NAMESPACE::D2, CI>::operator -(const _Vector<T, VSL_NAMESPACE::D2, CI>& p) const {
		return _Point<decltype(std::declval<R>() - std::declval<T>()), VSL_NAMESPACE::D2, CI>(value.___AN1 - p.value.___AN1, value.___AN2 - p.value.___AN2);
	}

	template<typename R, typename CI>
	template<concepts::multiply_as<R> T>
	constexpr auto _Point<R, VSL_NAMESPACE::D2, CI>::operator *(const T& m) const {
		return _Point<decltype(std::declval<R>()* m), VSL_NAMESPACE::D2, CI>(value.___AN1 * m, value.___AN2 * m);
	}

	template<typename R, typename CI>
	template<concepts::division_as<R> T>
	constexpr auto _Point<R, VSL_NAMESPACE::D2, CI>::operator /(const T& m) const {
		return _Point<decltype(std::declval<R>() / m), VSL_NAMESPACE::D2, CI>(value.___AN1 / m, value.___AN2 / m);
	}

	template<typename R, typename CI>
	template<concepts::add_as<R> T>
	constexpr _Point<R, VSL_NAMESPACE::D2, CI>& _Point<R, VSL_NAMESPACE::D2, CI>::operator +=(const _Vector<T, VSL_NAMESPACE::D2, CI>& p) {
		this->value.___AN1 += p.value.___AN1;
		this->value.___AN2 += p.value.___AN2;
		return *this;
	}

	template<typename R, typename CI>
	template<concepts::subtract_as<R> T>
	constexpr _Point<R, VSL_NAMESPACE::D2, CI>& _Point<R, VSL_NAMESPACE::D2, CI>::operator -=(const _Vector<T, VSL_NAMESPACE::D2, CI>& p) {
		this->value.___AN1 -= p.value.___AN1;
		this->value.___AN2 -= p.value.___AN2;
		return *this;
	}

	template<typename R, typename CI>
	template<concepts::multiply_as<R> T>
	constexpr _Point<R, VSL_NAMESPACE::D2, CI>& _Point<R, VSL_NAMESPACE::D2, CI>::operator *=(const T& m) {
		this->value.___AN1 *= m;
		this->value.___AN2 *= m;
		return *this;
	}

	template<typename R, typename CI>
	template<concepts::division_as<R> T>
	constexpr _Point<R, VSL_NAMESPACE::D2, CI>& _Point<R, VSL_NAMESPACE::D2, CI>::operator /=(const T& m) {
		this->value.___AN1 /= m;
		this->value.___AN2 /= m;
		return *this;
	}

	template<typename R, typename CI>
	template<concepts::cast_as<R> T>
	constexpr _Point<T, VSL_NAMESPACE::D2, CI> _Point<R, VSL_NAMESPACE::D2, CI>::cast() const
	{
		return _Point<T, VSL_NAMESPACE::D2, CI>((T)value.___AN1, (T)value.___AN2);
	}


	template<typename R, typename CI>
	constexpr auto _Point<R, VSL_NAMESPACE::D2, CI>::length() const
	{
		return std::sqrt(value.___AN1 * value.___AN1 + value.___AN2 * value.___AN2);
	}

	// ---------------------------------------------------------------------------------------------------------------------

	template<typename R, typename CI>
	constexpr bool _Point<R, VSL_NAMESPACE::D3, CI>::operator<(const _Point<R, VSL_NAMESPACE::D3, CI>& p) const
	{
		return length() < p.length();
	}

	template<typename R, typename CI>
	constexpr _Point<R, VSL_NAMESPACE::D3, CI> _Point<R, VSL_NAMESPACE::D3, CI>::operator +() const {
		return *this;
	}

	template<typename R, typename CI>
	constexpr _Point<R, VSL_NAMESPACE::D3, CI> _Point<R, VSL_NAMESPACE::D3, CI>::operator -() const {
		return _Point<R, VSL_NAMESPACE::D3, CI>(-value.___AN1, -value.___AN2, -value.___AN3);
	}

	template<typename R, typename CI>
	template<concepts::subtract_as<R> T>
	constexpr auto _Point<R, VSL_NAMESPACE::D3, CI>::operator -(const _Point<T, VSL_NAMESPACE::D3, CI>& p) const {
		return _Vector<decltype(std::declval<R>() - std::declval<T>()), VSL_NAMESPACE::D3, CI>(value.___AN1 - p.value.___AN1, value.___AN2 - p.value.___AN2, value.___AN3 - p.value.___AN3);
	}

	template<typename R, typename CI>
	template<concepts::add_as<R> T>
	constexpr auto _Point<R, VSL_NAMESPACE::D3, CI>::operator +(const _Vector<T, VSL_NAMESPACE::D3, CI>& p) const {
		return _Point<decltype(std::declval<R>() + std::declval<T>()), VSL_NAMESPACE::D3, CI>(value.___AN1 + p.value.___AN1, value.___AN2 + p.value.___AN2, value.___AN3 + p.value.___AN3);
	}

	template<typename R, typename CI>
	template<concepts::subtract_as<R> T>
	constexpr auto _Point<R, VSL_NAMESPACE::D3, CI>::operator -(const _Vector<T, VSL_NAMESPACE::D3, CI>& p) const {
		return _Point<decltype(std::declval<R>() - std::declval<T>()), VSL_NAMESPACE::D3, CI>(value.___AN1 - p.value.___AN1, value.___AN2 - p.value.___AN2, value.___AN3 - p.value.___AN3);
	}

	template<typename R, typename CI>
	template<concepts::multiply_as<R> T>
	constexpr auto _Point<R, VSL_NAMESPACE::D3, CI>::operator *(const T& m) const {
		return _Point<decltype(std::declval<R>()* m), VSL_NAMESPACE::D3, CI>(value.___AN1 * m, value.___AN2 * m, value.___AN3 * m);
	}

	template<typename R, typename CI>
	template<concepts::division_as<R> T>
	constexpr auto _Point<R, VSL_NAMESPACE::D3, CI>::operator /(const T& m) const {
		return _Point<decltype(std::declval<R>() / m), VSL_NAMESPACE::D3, CI>(value.___AN1 / m, value.___AN2 / m, value.___AN3 / m);
	}

	template<typename R, typename CI>
	template<concepts::add_as<R> T>
	constexpr _Point<R, VSL_NAMESPACE::D3, CI>& _Point<R, VSL_NAMESPACE::D3, CI>::operator +=(const _Vector<T, VSL_NAMESPACE::D3, CI>& p) {
		this->value.___AN1 += p.value.___AN1;
		this->value.___AN2 += p.value.___AN2;
		this->value.___AN3 += p.value.___AN3;
		return *this;
	}

	template<typename R, typename CI>
	template<concepts::subtract_as<R> T>
	constexpr _Point<R, VSL_NAMESPACE::D3, CI>& _Point<R, VSL_NAMESPACE::D3, CI>::operator -=(const _Vector<T, VSL_NAMESPACE::D3, CI>& p) {
		this->value.___AN1 -= p.value.___AN1;
		this->value.___AN2 -= p.value.___AN2;
		this->value.___AN3 -= p.value.___AN3;
		return *this;
	}

	template<typename R, typename CI>
	template<concepts::multiply_as<R> T>
	constexpr _Point<R, VSL_NAMESPACE::D3, CI>& _Point<R, VSL_NAMESPACE::D3, CI>::operator *=(const T& m) {
		this->value.___AN1 *= m;
		this->value.___AN2 *= m;
		this->value.___AN3 *= m;
		return *this;
	}

	template<typename R, typename CI>
	template<concepts::division_as<R> T>
	constexpr _Point<R, VSL_NAMESPACE::D3, CI>& _Point<R, VSL_NAMESPACE::D3, CI>::operator /=(const T& m) {
		this->value.___AN1 /= m;
		this->value.___AN2 /= m;
		this->value.___AN3 /= m;
		return *this;
	}

	template<typename R, typename CI>
	template<concepts::cast_as<R> T>
	constexpr _Point<T, VSL_NAMESPACE::D3, CI> _Point<R, VSL_NAMESPACE::D3, CI>::cast() const
	{
		return _Point<T, VSL_NAMESPACE::D3, CI>((T)value.___AN1, (T)value.___AN2);
	}


	template<typename R, typename CI>
	constexpr auto _Point<R, VSL_NAMESPACE::D3, CI>::length() const
	{
		return std::sqrt(value.___AN1 * value.___AN1 + value.___AN2 * value.___AN2 + value.___AN3 * value.___AN3);
	}
}