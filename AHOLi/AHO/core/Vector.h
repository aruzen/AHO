#pragma once
#include "../define.h"
#include "../concepts.h"

#include "../../VSL/dimention.h"

#include <compare>

namespace AHO_NAMESPACE {
	template<typename R = int, VSL_NAMESPACE::is_dimention D = VSL_NAMESPACE::VSL_DEFAULT_DIMENTION_STRUCT>
	struct _Vector {
	};

	template<typename R = int, typename... Args>
		requires sames_as<R, Args...>
	struct Vector : public _Vector<R, typename VSL_NAMESPACE::Dimention<sizeof...(Args) + 1>::value> {
		constexpr Vector() {};

		constexpr Vector(R x, Args... args) : _Vector<R, typename VSL_NAMESPACE::Dimention<sizeof...(Args) + 1>::value>(x, args...) {};
	};

	template<typename T>
	concept is_vector = requires{
		typename T::dimention;
		typename T::element_type;
		std::convertible_to<T, _Vector<typename T::element_type, typename T::dimention>>;
	};

	namespace d1 {
		template<typename R>
		struct Vector : public AHO_NAMESPACE::_Vector<R, VSL_NAMESPACE::D1> {
			constexpr Vector() {};

			constexpr Vector(R x) : AHO_NAMESPACE::_Vector<R, VSL_NAMESPACE::D1>(x) {};
		};
		using VectorI = AHO_NAMESPACE::_Vector<int, VSL_NAMESPACE::D1>;
		using VectorF = AHO_NAMESPACE::_Vector<float, VSL_NAMESPACE::D1>;
		using VectorD = AHO_NAMESPACE::_Vector<double, VSL_NAMESPACE::D1>;
	}

	namespace d2 {
		template<typename R>
		struct Vector : public AHO_NAMESPACE::_Vector<R, VSL_NAMESPACE::D2> {
			constexpr Vector() {};

			constexpr Vector(R x, R y) : AHO_NAMESPACE::_Vector<R, VSL_NAMESPACE::D2>(x, y) {};
		};
		using VectorI = AHO_NAMESPACE::_Vector<int, VSL_NAMESPACE::D2>;
		using VectorF = AHO_NAMESPACE::_Vector<float, VSL_NAMESPACE::D2>;
		using VectorD = AHO_NAMESPACE::_Vector<double, VSL_NAMESPACE::D2>;
	}

	namespace d3 {
		template<typename R>
		struct Vector : public AHO_NAMESPACE::_Vector<R, VSL_NAMESPACE::D3> {
			constexpr Vector() {};

			constexpr Vector(R x, R y, R z) : AHO_NAMESPACE::_Vector<R, VSL_NAMESPACE::D3>(x, y, z) {};
		};
		using VectorI = AHO_NAMESPACE::_Vector<int, VSL_NAMESPACE::D3>;
		using VectorF = AHO_NAMESPACE::_Vector<float, VSL_NAMESPACE::D3>;
		using VectorD = AHO_NAMESPACE::_Vector<double, VSL_NAMESPACE::D3>;
	}

	template<typename R>
	struct _Vector<R, VSL_NAMESPACE::D1> {
		using dimention = typename VSL_NAMESPACE::D1;
		using element_type = R;

		R x;

		constexpr _Vector() {};

		constexpr _Vector(R x) : x(x) {};

		constexpr bool operator< (const _Vector<R, VSL_NAMESPACE::D1>& p) const;

		constexpr auto operator <=>(const _Vector<R, VSL_NAMESPACE::D1>&) const = default;

		constexpr _Vector<R, VSL_NAMESPACE::D1> operator +() const;

		constexpr _Vector<R, VSL_NAMESPACE::D1> operator -() const;

		template<add_as<R> T>
		constexpr auto operator +(const _Vector<T, VSL_NAMESPACE::D1>& p) const;

		template<subtract_as<R> T>
		constexpr auto operator -(const _Vector<T, VSL_NAMESPACE::D1>& p) const;

		template<multiply_as<R> T>
			requires (!is_vector<T>)
		constexpr auto operator *(const T& m) const;

		template<typename T>
			requires is_vector<T>&& std::convertible_to<_Vector<typename T::element_type, VSL_NAMESPACE::D1>, T>
		constexpr auto operator *(const T& m) const;

		template<division_as<R> T>
		constexpr auto operator /(const T& m) const;

		template<add_as<R> T>
		constexpr _Vector<R, VSL_NAMESPACE::D1>& operator +=(const _Vector<T, VSL_NAMESPACE::D1>& p) const;

		template<subtract_as<R> T>
		constexpr _Vector<R, VSL_NAMESPACE::D1>& operator -=(const _Vector<T, VSL_NAMESPACE::D1>& p) const;

		template<multiply_as<R> T>
		constexpr _Vector<R, VSL_NAMESPACE::D1>& operator *=(const T& m) const;

		template<division_as<R> T>
		constexpr _Vector<R, VSL_NAMESPACE::D1>& operator /=(const T& m) const;

		template<cast_as<R> T>
		constexpr _Vector<T, VSL_NAMESPACE::D1> cast() const;

		constexpr auto length() const;
	};

	template<typename R>
	struct _Vector<R, VSL_NAMESPACE::D2> {
		using dimention = typename VSL_NAMESPACE::D2;
		using element_type = R;

		R x, y;

		constexpr _Vector() {};

		constexpr _Vector(R x, R y) : x(x), y(y) {};

		constexpr bool operator< (const _Vector<R, VSL_NAMESPACE::D2>& p) const;

		constexpr auto operator <=>(const _Vector<R, VSL_NAMESPACE::D2>&) const = default;

		constexpr _Vector<R, VSL_NAMESPACE::D2> operator +() const;

		constexpr _Vector<R, VSL_NAMESPACE::D2> operator -() const;

		template<add_as<R> T>
		constexpr auto operator +(const _Vector<T, VSL_NAMESPACE::D2>& p) const;

		template<subtract_as<R> T>
		constexpr auto operator -(const _Vector<T, VSL_NAMESPACE::D2>& p) const;

		template<multiply_as<R> T>
			requires (!is_vector<T>)
		constexpr auto operator *(const T& m) const;

		template<typename T>
			requires is_vector<T>&& std::convertible_to<_Vector<typename T::element_type, VSL_NAMESPACE::D2>, T>
		constexpr auto operator *(const T& m) const;

		template<division_as<R> T>
		constexpr auto operator /(const T& m) const;

		template<add_as<R> T>
		constexpr _Vector<R, VSL_NAMESPACE::D2>& operator +=(const _Vector<T, VSL_NAMESPACE::D2>& p) const;

		template<subtract_as<R> T>
		constexpr _Vector<R, VSL_NAMESPACE::D2>& operator -=(const _Vector<T, VSL_NAMESPACE::D2>& p) const;

		template<multiply_as<R> T>
		constexpr _Vector<R, VSL_NAMESPACE::D2>& operator *=(const T& m) const;

		template<division_as<R> T>
		constexpr _Vector<R, VSL_NAMESPACE::D2>& operator /=(const T& m) const;

		template<cast_as<R> T>
		constexpr _Vector<T, VSL_NAMESPACE::D2> cast() const;

		constexpr auto length() const;
	};

	template<typename R>
	struct _Vector<R, VSL_NAMESPACE::D3> {
		using dimention = typename VSL_NAMESPACE::D3;
		using element_type = R;

		R x, y, z;

		constexpr _Vector() {};

		constexpr _Vector(R x, R y, R z) : x(x), y(y), z(z) {};

		constexpr bool operator< (const _Vector<R, VSL_NAMESPACE::D3>& p) const;

		constexpr auto operator <=>(const _Vector<R, VSL_NAMESPACE::D3>&) const = default;

		constexpr _Vector<R, VSL_NAMESPACE::D3> operator +() const;

		constexpr _Vector<R, VSL_NAMESPACE::D3> operator -() const;

		template<add_as<R> T>
		constexpr auto operator +(const _Vector<T, VSL_NAMESPACE::D3>& p) const;

		template<subtract_as<R> T>
		constexpr auto operator -(const _Vector<T, VSL_NAMESPACE::D3>& p) const;

		template<multiply_as<R> T>
			requires (!is_vector<T>)
		constexpr auto operator *(const T& m) const;

		template<typename T>
			requires is_vector<T>&& std::convertible_to<_Vector<typename T::element_type, VSL_NAMESPACE::D3>, T>
		constexpr auto operator *(const T& m) const;

		template<division_as<R> T>
		constexpr auto operator /(const T& m) const;

		template<add_as<R> T>
		constexpr _Vector<R, VSL_NAMESPACE::D3>& operator +=(const _Vector<T, VSL_NAMESPACE::D3>& p) const;

		template<subtract_as<R> T>
		constexpr _Vector<R, VSL_NAMESPACE::D3>& operator -=(const _Vector<T, VSL_NAMESPACE::D3>& p) const;

		template<multiply_as<R> T>
		constexpr _Vector<R, VSL_NAMESPACE::D3>& operator *=(const T& m) const;

		template<division_as<R> T>
		constexpr _Vector<R, VSL_NAMESPACE::D3>& operator /=(const T& m) const;

		template<cast_as<R> T>
		constexpr _Vector<T, VSL_NAMESPACE::D3> cast() const;

		constexpr auto length() const;
	};

	// ---------------------------------------------------------------------------------------------------------------------

	template<typename R>
	constexpr bool _Vector<R, VSL_NAMESPACE::D1>::operator<(const _Vector<R, VSL_NAMESPACE::D1>& p) const
	{
		return length() < p.length();
	}

	template<typename R>
	constexpr _Vector<R, VSL_NAMESPACE::D1> _Vector<R, VSL_NAMESPACE::D1>::operator +() const {
		return *this;
	}

	template<typename R>
	constexpr _Vector<R, VSL_NAMESPACE::D1> _Vector<R, VSL_NAMESPACE::D1>::operator -() const {
		return _Vector<R, VSL_NAMESPACE::D1>(-x);
	}

	template<typename R>
	template<add_as<R> T>
	constexpr auto _Vector<R, VSL_NAMESPACE::D1>::operator +(const _Vector<T, VSL_NAMESPACE::D1>& p) const {
		return _Vector<decltype(x + p.x), VSL_NAMESPACE::D1>(x + p.x);
	}

	template<typename R>
	template<subtract_as<R> T>
	constexpr auto _Vector<R, VSL_NAMESPACE::D1>::operator -(const _Vector<T, VSL_NAMESPACE::D1>& p) const {
		return _Vector<decltype(x - p.x), VSL_NAMESPACE::D1>(x - p.x);
	}

	template<typename R>
	template<multiply_as<R> T>
		requires (!is_vector<T>)
	constexpr auto _Vector<R, VSL_NAMESPACE::D1>::operator *(const T& m) const {
		return _Vector<decltype(x* m), VSL_NAMESPACE::D1>(x * m);
	}

	template<typename R>
	template<typename T>
		requires is_vector<T>&& std::convertible_to<_Vector<typename T::element_type, VSL_NAMESPACE::D1>, T>
	inline constexpr auto _Vector<R, VSL_NAMESPACE::D1>::operator*(const T& m) const
	{
		return (decltype(x * m.x))0;
	}

	template<typename R>
	template<division_as<R> T>
	constexpr auto _Vector<R, VSL_NAMESPACE::D1>::operator /(const T& m) const {
		return _Vector<decltype(x / m), VSL_NAMESPACE::D1>(x / m);
	}

	template<typename R>
	template<add_as<R> T>
	constexpr _Vector<R, VSL_NAMESPACE::D1>& _Vector<R, VSL_NAMESPACE::D1>::operator +=(const _Vector<T, VSL_NAMESPACE::D1>& p) const {
		this->x += p.x;
		return *this;
	}

	template<typename R>
	template<subtract_as<R> T>
	constexpr _Vector<R, VSL_NAMESPACE::D1>& _Vector<R, VSL_NAMESPACE::D1>::operator -=(const _Vector<T, VSL_NAMESPACE::D1>& p) const {
		this->x -= p.x;
		return *this;
	}

	template<typename R>
	template<multiply_as<R> T>
	constexpr _Vector<R, VSL_NAMESPACE::D1>& _Vector<R, VSL_NAMESPACE::D1>::operator *=(const T& p) const {
		this->x *= p;
		return *this;
	}

	template<typename R>
	template<division_as<R> T>
	constexpr _Vector<R, VSL_NAMESPACE::D1>& _Vector<R, VSL_NAMESPACE::D1>::operator /=(const T& p) const {
		this->x /= p;
		return *this;
	}

	template<typename R>
	template<cast_as<R> T>
	constexpr _Vector<T, VSL_NAMESPACE::D1> _Vector<R, VSL_NAMESPACE::D1>::cast() const
	{
		return _Vector<T, VSL_NAMESPACE::D1>((T)x);
	}


	template<typename R>
	constexpr auto _Vector<R, VSL_NAMESPACE::D1>::length() const
	{
		return x;
	}

	// ---------------------------------------------------------------------------------------------------------------------

	template<typename R>
	constexpr bool _Vector<R, VSL_NAMESPACE::D2>::operator<(const _Vector<R, VSL_NAMESPACE::D2>& p) const
	{
		return length() < p.length();
	}

	template<typename R>
	constexpr _Vector<R, VSL_NAMESPACE::D2> _Vector<R, VSL_NAMESPACE::D2>::operator +() const {
		return *this;
	}

	template<typename R>
	constexpr _Vector<R, VSL_NAMESPACE::D2> _Vector<R, VSL_NAMESPACE::D2>::operator -() const {
		return _Vector<R, VSL_NAMESPACE::D2>(-x, -y);
	}

	template<typename R>
	template<add_as<R> T>
	constexpr auto _Vector<R, VSL_NAMESPACE::D2>::operator +(const _Vector<T, VSL_NAMESPACE::D2>& p) const {
		return _Vector<decltype(x + p.x), VSL_NAMESPACE::D2>(x + p.x, y + p.y);
	}

	template<typename R>
	template<subtract_as<R> T>
	constexpr auto _Vector<R, VSL_NAMESPACE::D2>::operator -(const _Vector<T, VSL_NAMESPACE::D2>& p) const {
		return _Vector<decltype(x - p.x), VSL_NAMESPACE::D2>(x - p.x, y - p.y);
	}

	template<typename R>
	template<multiply_as<R> T>
		requires (!is_vector<T>)
	constexpr auto _Vector<R, VSL_NAMESPACE::D2>::operator *(const T& m) const {
		return _Vector<decltype(x * m), VSL_NAMESPACE::D2>(x * m, y * m);
	}

	template<typename R>
	template<typename T>
		requires is_vector<T>&& std::convertible_to<_Vector<typename T::element_type, VSL_NAMESPACE::D2>, T>
	inline constexpr auto _Vector<R, VSL_NAMESPACE::D2>::operator*(const T& m) const
	{
		return x * m.x + y * m.y;
	}

	template<typename R>
	template<division_as<R> T>
	constexpr auto _Vector<R, VSL_NAMESPACE::D2>::operator /(const T& m) const {
		return _Vector<decltype(x / m), VSL_NAMESPACE::D2>(x / m, y / m);
	}

	template<typename R>
	template<add_as<R> T>
	constexpr _Vector<R, VSL_NAMESPACE::D2>& _Vector<R, VSL_NAMESPACE::D2>::operator +=(const _Vector<T, VSL_NAMESPACE::D2>& p) const {
		this->x += p.x;
		this->y += p.y;
		return *this;
	}

	template<typename R>
	template<subtract_as<R> T>
	constexpr _Vector<R, VSL_NAMESPACE::D2>& _Vector<R, VSL_NAMESPACE::D2>::operator -=(const _Vector<T, VSL_NAMESPACE::D2>& p) const {
		this->x -= p.x;
		this->y -= p.y;
		return *this;
	}

	template<typename R>
	template<multiply_as<R> T>
	constexpr _Vector<R, VSL_NAMESPACE::D2>& _Vector<R, VSL_NAMESPACE::D2>::operator *=(const T& p) const {
		this->x *= p;
		this->y *= p;
		return *this;
	}

	template<typename R>
	template<division_as<R> T>
	constexpr _Vector<R, VSL_NAMESPACE::D2>& _Vector<R, VSL_NAMESPACE::D2>::operator /=(const T& p) const {
		this->x /= p;
		this->y /= p;
		return *this;
	}

	template<typename R>
	template<cast_as<R> T>
	constexpr _Vector<T, VSL_NAMESPACE::D2> _Vector<R, VSL_NAMESPACE::D2>::cast() const
	{
		return _Vector<T, VSL_NAMESPACE::D2>((T)x, (T)y);
	}


	template<typename R>
	constexpr auto _Vector<R, VSL_NAMESPACE::D2>::length() const
	{
		return std::sqrt(x * x + y * y);
	}

	// ---------------------------------------------------------------------------------------------------------------------

	template<typename R>
	constexpr bool _Vector<R, VSL_NAMESPACE::D3>::operator<(const _Vector<R, VSL_NAMESPACE::D3>& p) const
	{
		return length() < p.length();
	}

	template<typename R>
	constexpr _Vector<R, VSL_NAMESPACE::D3> _Vector<R, VSL_NAMESPACE::D3>::operator +() const {
		return *this;
	}

	template<typename R>
	constexpr _Vector<R, VSL_NAMESPACE::D3> _Vector<R, VSL_NAMESPACE::D3>::operator -() const {
		return _Vector<R, VSL_NAMESPACE::D3>(-x, -y, -z);
	}

	template<typename R>
	template<add_as<R> T>
	constexpr auto _Vector<R, VSL_NAMESPACE::D3>::operator +(const _Vector<T, VSL_NAMESPACE::D3>& p) const {
		return _Vector<decltype(x + p.x), VSL_NAMESPACE::D3>(x + p.x, y + p.y, z + p.z);
	}

	template<typename R>
	template<subtract_as<R> T>
	constexpr auto _Vector<R, VSL_NAMESPACE::D3>::operator -(const _Vector<T, VSL_NAMESPACE::D3>& p) const {
		return _Vector<decltype(x - p.x), VSL_NAMESPACE::D3>(x - p.x, y - p.y, z - p.z);
	}

	template<typename R>
	template<multiply_as<R> T>
		requires (!is_vector<T>)
	constexpr auto _Vector<R, VSL_NAMESPACE::D3>::operator *(const T& m) const {
		return _Vector<decltype(x* m), VSL_NAMESPACE::D3>(x * m, y * m, z * m);
	}

	template<typename R>
	template<typename T>
		requires is_vector<T>&& std::convertible_to<_Vector<typename T::element_type, VSL_NAMESPACE::D3>, T>
	inline constexpr auto _Vector<R, VSL_NAMESPACE::D3>::operator *(const T& m) const
	{
		return x * m.x + y * m.y + z * m.z;
	}

	template<typename R>
	template<division_as<R> T>
	constexpr auto _Vector<R, VSL_NAMESPACE::D3>::operator /(const T& m) const {
		return _Vector<decltype(x / m), VSL_NAMESPACE::D3>(x / m, y / m, z / m);
	}

	template<typename R>
	template<add_as<R> T>
	constexpr _Vector<R, VSL_NAMESPACE::D3>& _Vector<R, VSL_NAMESPACE::D3>::operator +=(const _Vector<T, VSL_NAMESPACE::D3>& p) const {
		this->x += p.x;
		this->y += p.y;
		this->z += p.z;
		return *this;
	}

	template<typename R>
	template<subtract_as<R> T>
	constexpr _Vector<R, VSL_NAMESPACE::D3>& _Vector<R, VSL_NAMESPACE::D3>::operator -=(const _Vector<T, VSL_NAMESPACE::D3>& p) const {
		this->x -= p.x;
		this->y -= p.y;
		this->z -= p.z;
		return *this;
	}

	template<typename R>
	template<multiply_as<R> T>
	constexpr _Vector<R, VSL_NAMESPACE::D3>& _Vector<R, VSL_NAMESPACE::D3>::operator *=(const T& p) const {
		this->x *= p;
		this->y *= p;
		this->z *= p;
		return *this;
	}

	template<typename R>
	template<division_as<R> T>
	constexpr _Vector<R, VSL_NAMESPACE::D3>& _Vector<R, VSL_NAMESPACE::D3>::operator /=(const T& p) const {
		this->x /= p;
		this->y /= p;
		this->z /= p;
		return *this;
	}

	template<typename R>
	template<cast_as<R> T>
	constexpr _Vector<T, VSL_NAMESPACE::D3> _Vector<R, VSL_NAMESPACE::D3>::cast() const
	{
		return _Vector<T, VSL_NAMESPACE::D3>((T)x, (T)y, (T)z);
	}

	template<typename R>
	constexpr auto _Vector<R, VSL_NAMESPACE::D3>::length() const
	{
		return std::sqrt(x * x + y * y + z * z);
	}
}