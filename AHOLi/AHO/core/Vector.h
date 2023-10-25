#pragma once
#include "../define.h"
#include "../concepts.h"

#include <VSL/dimention.h>

#include <compare>

namespace AHO_NAMESPACE {
	template<typename R = int,
			 VSL_NAMESPACE::is_dimention D = VSL_NAMESPACE::VSL_DEFAULT_DIMENTION_STRUCT,
	 		 typename CoordinateInfo = AHO_NAMESPACE::coordinate::_DefaultCoordinateInfo<D>::value>
	struct _Vector {
	};

	template<typename R = int, typename... Args>
		requires concepts::sames_as<R, Args...>
	struct Vector : public _Vector<R, 
								   typename VSL_NAMESPACE::Dimention<sizeof...(Args) + 1>::value, 
								   typename AHO_NAMESPACE::coordinate::_DefaultCoordinateInfo<
								       typename VSL_NAMESPACE::Dimention<sizeof...(Args) + 1>::value
								   >::value> {
		constexpr Vector() {};

		constexpr Vector(R x, Args... args) : _Vector<R,
											  typename VSL_NAMESPACE::Dimention<sizeof...(Args) + 1>::value,
											  typename AHO_NAMESPACE::coordinate::_DefaultCoordinateInfo<
											      typename VSL_NAMESPACE::Dimention<sizeof...(Args) + 1>::value
											  >::value>(x, args...) {};
	};

	namespace concepts {
		template<typename T>
		concept is_vector = requires{
			typename T::dimention;
			typename T::element_type;
			std::convertible_to<T, _Vector<typename T::element_type, typename T::dimention>>;
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
		using dimention = typename VSL_NAMESPACE::D1;
		using element_type = R;

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

		template<typename T>
			requires concepts::is_vector<T>&& std::convertible_to<_Vector<typename T::element_type, VSL_NAMESPACE::D1>, T>
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

		constexpr auto length() const;
	};

	template<typename R, typename CI>
	struct _Vector<R, VSL_NAMESPACE::D2, CI> {
		using dimention = typename VSL_NAMESPACE::D2;
		using element_type = R;

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

		template<typename T>
			requires concepts::is_vector<T>&& std::convertible_to<_Vector<typename T::element_type, VSL_NAMESPACE::D2>, T>
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

		constexpr auto length() const;
	};

	template<typename R, typename CI>
	struct _Vector<R, VSL_NAMESPACE::D3, CI> {
		using dimention = typename VSL_NAMESPACE::D3;
		using element_type = R;

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

		template<typename T>
			requires concepts::is_vector<T>&& std::convertible_to<_Vector<typename T::element_type, VSL_NAMESPACE::D3, CI>, T>
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

		constexpr auto length() const;
	};

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
	template<typename T>
		requires concepts::is_vector<T>&& std::convertible_to<_Vector<typename T::element_type, VSL_NAMESPACE::D1>, T>
	inline constexpr auto _Vector<R, VSL_NAMESPACE::D1, CI>::operator*(const T& m) const
	{
		return (decltype(std::declval<R>() * std::declval<T>()))0;
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
	constexpr auto _Vector<R, VSL_NAMESPACE::D1, CI>::length() const
	{
		return value.___AN1;
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
	template<typename T>
		requires concepts::is_vector<T>&& std::convertible_to<_Vector<typename T::element_type, VSL_NAMESPACE::D2>, T>
	inline constexpr auto _Vector<R, VSL_NAMESPACE::D2, CI>::operator*(const T& m) const
	{
		return value.___AN1 * m.value.___AN1 + value.___AN2 * m.value.___AN2;
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
	constexpr auto _Vector<R, VSL_NAMESPACE::D2, CI>::length() const
	{
		return std::sqrt(value.___AN1 * value.___AN1 + value.___AN2 * value.___AN2);
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
	template<typename T>
		requires concepts::is_vector<T>&& std::convertible_to<_Vector<typename T::element_type, VSL_NAMESPACE::D3, CI>, T>
	inline constexpr auto _Vector<R, VSL_NAMESPACE::D3, CI>::operator *(const T& m) const
	{
		return value.___AN1 * m.value.___AN1 + value.___AN2 * m.value.___AN2 + value.___AN3 * m.value.___AN3;
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
	constexpr auto _Vector<R, VSL_NAMESPACE::D3, CI>::length() const
	{
		return std::sqrt(value.___AN1 * value.___AN1 + value.___AN2 * value.___AN2 + value.___AN3 * value.___AN3);
	}
}