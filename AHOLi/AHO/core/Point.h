#pragma once
#include "../define.h"
#include "../concepts.h"

#include "../../VSL/dimention.h"

#include "math/coordinate.h"

#include "Vector.h"

namespace AHO_NAMESPACE {
	template<typename R = int, 
		VSL_NAMESPACE::is_dimention D = VSL_NAMESPACE::VSL_DEFAULT_DIMENTION_STRUCT /*,
		typename CoordinateInfo = AHO_NAMESPACE::coordinate::_DefaultCoordinateInfo<D>::value*/>
	struct _Point {
	};

	template<typename T>
	concept is_point = requires(T t) {
		typename std::remove_reference_t<T>::dimention;
		typename std::remove_reference_t<T>::element_type;
		std::convertible_to<std::remove_reference_t<T>, _Point<typename std::remove_reference_t<T>::element_type, typename std::remove_reference_t<T>::dimention>>;
		(_Point<typename std::remove_reference_t<T>::element_type, typename std::remove_reference_t<T>::dimention>)t;
	};

	template<typename R = int, typename... Args>
		requires sames_as<R, Args...>
	struct Point : public _Point<R, typename VSL_NAMESPACE::Dimention<sizeof...(Args) + 1>::value> {
		constexpr Point() {};

		constexpr Point(R x, Args... args) : _Point<R, typename VSL_NAMESPACE::Dimention<sizeof...(Args) + 1>::value>(x, args...) {};
	};

	namespace d1 {
		template<typename R>
		struct Point : public AHO_NAMESPACE::_Point<R, VSL_NAMESPACE::D1> {
			constexpr Point() {};
			
			constexpr Point(R x) : AHO_NAMESPACE::_Point<R, VSL_NAMESPACE::D1>(x) {};
		};
		using PointI = AHO_NAMESPACE::_Point<int, VSL_NAMESPACE::D1>;
		using PointF = AHO_NAMESPACE::_Point<float, VSL_NAMESPACE::D1>;
		using PointD = AHO_NAMESPACE::_Point<double, VSL_NAMESPACE::D1>;
	}

	namespace d2 {
		template<typename R>
		struct Point : public AHO_NAMESPACE::_Point<R, VSL_NAMESPACE::D2> {
			constexpr Point() {};
			
			constexpr Point(R x, R y) : AHO_NAMESPACE::_Point<R, VSL_NAMESPACE::D2>(x, y) {};
		};
		using PointI = AHO_NAMESPACE::_Point<int, VSL_NAMESPACE::D2>;
		using PointF = AHO_NAMESPACE::_Point<float, VSL_NAMESPACE::D2>;
		using PointD = AHO_NAMESPACE::_Point<double, VSL_NAMESPACE::D2>;
	}

	namespace d3 {
		template<typename R>
		struct Point : public AHO_NAMESPACE::_Point<R, VSL_NAMESPACE::D3> {
			constexpr Point() {};
			
			constexpr Point(R x, R y, R z) : AHO_NAMESPACE::_Point<R, VSL_NAMESPACE::D3>(x, y, z) {};
		};
		using PointI = AHO_NAMESPACE::_Point<int, VSL_NAMESPACE::D3>;
		using PointF = AHO_NAMESPACE::_Point<float, VSL_NAMESPACE::D3>;
		using PointD = AHO_NAMESPACE::_Point<double, VSL_NAMESPACE::D3>;
	}

	template<typename R>
	struct _Point<R, VSL_NAMESPACE::D1> {
		using dimention = typename VSL_NAMESPACE::D1;
		using element_type = R;

		R x;

		constexpr _Point() {};

		constexpr _Point(R x) : x(x) {};

		constexpr bool operator< (const _Point<R, VSL_NAMESPACE::D1>& p) const;

		constexpr auto operator <=>(const _Point<R, VSL_NAMESPACE::D1>&) const = default;

		constexpr _Point<R, VSL_NAMESPACE::D1> operator +() const;

		constexpr _Point<R, VSL_NAMESPACE::D1> operator -() const;

		template<subtract_as<R> T>
		constexpr auto /*Vector*/ operator -(const _Point<T, VSL_NAMESPACE::D1>& p) const;

		template<add_as<R> T>
		constexpr auto /*Point*/ operator +(const _Vector<T, VSL_NAMESPACE::D1>& p) const;

		template<subtract_as<R> T>
		constexpr auto /*Point*/ operator -(const _Vector<T, VSL_NAMESPACE::D1>& p) const;

		template<multiply_as<R> T>
		constexpr auto /*Point*/ operator *(const T& m) const;

		template<division_as<R> T>
		constexpr auto /*Point*/ operator /(const T& m) const;

		template<add_as<R> T>
		constexpr _Point<R, VSL_NAMESPACE::D1>& /*Point*/ operator +=(const _Vector<T, VSL_NAMESPACE::D1>& p) const;

		template<subtract_as<R> T>
		constexpr _Point<R, VSL_NAMESPACE::D1>& /*Point*/ operator -=(const _Vector<T, VSL_NAMESPACE::D1>& p) const;

		template<multiply_as<R> T>
		constexpr _Point<R, VSL_NAMESPACE::D1>& /*Point*/ operator *=(const T& m) const;

		template<division_as<R> T>
		constexpr _Point<R, VSL_NAMESPACE::D1>& /*Point*/ operator /=(const T& m) const;

		template<cast_as<R> T>
		constexpr _Point<T, VSL_NAMESPACE::D1> cast() const;

		constexpr auto length() const;
	};

	template<typename R>
	struct _Point<R, VSL_NAMESPACE::D2> {
		using dimention = typename VSL_NAMESPACE::D2;
		using element_type = R;

		R x, y;

		constexpr _Point() {};

		constexpr _Point(R x, R y) : x(x), y(y) {};

		constexpr bool operator< (const _Point<R, VSL_NAMESPACE::D2>& p) const;

		constexpr auto operator <=>(const _Point<R, VSL_NAMESPACE::D2>&) const = default;

		constexpr _Point<R, VSL_NAMESPACE::D2> operator +() const;

		constexpr _Point<R, VSL_NAMESPACE::D2> operator -() const;

		template<subtract_as<R> T>
		constexpr auto /*Vector*/ operator -(const _Point<T, VSL_NAMESPACE::D2>& p) const;

		template<add_as<R> T>
		constexpr auto /*Point*/ operator +(const _Vector<T, VSL_NAMESPACE::D2>& p) const;

		template<subtract_as<R> T>
		constexpr auto /*Point*/ operator -(const _Vector<T, VSL_NAMESPACE::D2>& p) const;

		template<multiply_as<R> T>
		constexpr auto /*Point*/ operator *(const T& m) const;

		template<division_as<R> T>
		constexpr auto /*Point*/ operator /(const T& m) const;

		template<add_as<R> T>
		constexpr _Point<R, VSL_NAMESPACE::D2>& /*Point*/ operator +=(const _Vector<T, VSL_NAMESPACE::D2>& p) const;

		template<subtract_as<R> T>
		constexpr _Point<R, VSL_NAMESPACE::D2>& /*Point*/ operator -=(const _Vector<T, VSL_NAMESPACE::D2>& p) const;

		template<multiply_as<R> T>
		constexpr _Point<R, VSL_NAMESPACE::D2>& /*Point*/ operator *=(const T& m) const;

		template<division_as<R> T>
		constexpr _Point<R, VSL_NAMESPACE::D2>& /*Point*/ operator /=(const T& m) const;

		template<cast_as<R> T>
		constexpr _Point<T, VSL_NAMESPACE::D2> cast() const;

		constexpr auto length() const;
	};

	template<typename R>
	struct _Point<R, VSL_NAMESPACE::D3> {
		using dimention = typename VSL_NAMESPACE::D3;
		using element_type = R;

		R x, y, z;

		constexpr _Point() {};

		constexpr _Point(R x, R y, R z) : x(x), y(y), z(z) {};

		constexpr bool operator< (const _Point<R, VSL_NAMESPACE::D3>& p) const;

		constexpr auto operator <=>(const _Point<R, VSL_NAMESPACE::D3>&) const = default;

		constexpr _Point<R, VSL_NAMESPACE::D3> operator +() const;

		constexpr _Point<R, VSL_NAMESPACE::D3> operator -() const;

		template<subtract_as<R> T>
		constexpr auto /*Vector*/ operator -(const _Point<T, VSL_NAMESPACE::D3>& p) const;

		template<add_as<R> T>
		constexpr auto /*Point*/ operator +(const _Vector<T, VSL_NAMESPACE::D3>& p) const;

		template<subtract_as<R> T>
		constexpr auto /*Point*/ operator -(const _Vector<T, VSL_NAMESPACE::D3>& p) const;

		template<multiply_as<R> T>
		constexpr auto /*Point*/ operator *(const T& m) const;

		template<division_as<R> T>
		constexpr auto /*Point*/ operator /(const T& m) const;

		template<add_as<R> T>
		constexpr _Point<R, VSL_NAMESPACE::D3>& /*Point*/ operator +=(const _Vector<T, VSL_NAMESPACE::D3>& p) const;

		template<subtract_as<R> T>
		constexpr _Point<R, VSL_NAMESPACE::D3>& /*Point*/ operator -=(const _Vector<T, VSL_NAMESPACE::D3>& p) const;

		template<multiply_as<R> T>
		constexpr _Point<R, VSL_NAMESPACE::D3>& /*Point*/ operator *=(const T& m) const;

		template<division_as<R> T>
		constexpr _Point<R, VSL_NAMESPACE::D3>& /*Point*/ operator /=(const T& m) const;

		template<cast_as<R> T>
		constexpr _Point<T, VSL_NAMESPACE::D3> cast() const;

		constexpr auto length() const;
	};

	// ---------------------------------------------------------------------------------------------------------------------

	template<typename R>
	constexpr bool _Point<R, VSL_NAMESPACE::D1>::operator<(const _Point<R, VSL_NAMESPACE::D1>& p) const
	{
		return length() < p.length();
	}

	template<typename R>
	constexpr _Point<R, VSL_NAMESPACE::D1> _Point<R, VSL_NAMESPACE::D1>::operator +() const {
		return *this;
	}

	template<typename R>
	constexpr _Point<R, VSL_NAMESPACE::D1> _Point<R, VSL_NAMESPACE::D1>::operator -() const {
		return _Point<R, VSL_NAMESPACE::D1>(-x);
	}

	template<typename R>
	template<subtract_as<R> T>
	constexpr auto _Point<R, VSL_NAMESPACE::D1>::operator -(const _Point<T, VSL_NAMESPACE::D1>& p) const {
		return _Vector<decltype(x - p.x), VSL_NAMESPACE::D1>(x - p.x);
	}

	template<typename R>
	template<add_as<R> T>
	constexpr auto _Point<R, VSL_NAMESPACE::D1>::operator +(const _Vector<T, VSL_NAMESPACE::D1>& p) const {
		return _Point<decltype(x + p.x), VSL_NAMESPACE::D1>(x + p.x);
	}

	template<typename R>
	template<subtract_as<R> T>
	constexpr auto _Point<R, VSL_NAMESPACE::D1>::operator -(const _Vector<T, VSL_NAMESPACE::D1>& p) const {
		return _Point<decltype(x - p.x), VSL_NAMESPACE::D1>(x - p.x);
	}

	template<typename R>
	template<multiply_as<R> T>
	constexpr auto _Point<R, VSL_NAMESPACE::D1>::operator *(const T& m) const {
		return _Point<decltype(x* m), VSL_NAMESPACE::D1>(x * m);
	}

	template<typename R>
	template<division_as<R> T>
	constexpr auto _Point<R, VSL_NAMESPACE::D1>::operator /(const T& m) const {
		return _Point<decltype(x / m), VSL_NAMESPACE::D1>(x / m);
	}

	template<typename R>
	template<add_as<R> T>
	constexpr _Point<R, VSL_NAMESPACE::D1>& _Point<R, VSL_NAMESPACE::D1>::operator +=(const _Vector<T, VSL_NAMESPACE::D1>& p) const {
		this->x += p.x;
		return *this;
	}

	template<typename R>
	template<subtract_as<R> T>
	constexpr _Point<R, VSL_NAMESPACE::D1>& _Point<R, VSL_NAMESPACE::D1>::operator -=(const _Vector<T, VSL_NAMESPACE::D1>& p) const {
		this->x -= p.x; 
		return *this;
	}

	template<typename R>
	template<multiply_as<R> T>
	constexpr _Point<R, VSL_NAMESPACE::D1>& _Point<R, VSL_NAMESPACE::D1>::operator *=(const T& m) const {
		this->x *= m; 
		return *this;
	}

	template<typename R>
	template<division_as<R> T>
	constexpr _Point<R, VSL_NAMESPACE::D1>& _Point<R, VSL_NAMESPACE::D1>::operator /=(const T& m) const {
		this->x /= m; 
		return *this;
	}

	template<typename R>
	template<cast_as<R> T>
	constexpr _Point<T, VSL_NAMESPACE::D1> _Point<R, VSL_NAMESPACE::D1>::cast() const
	{
		return _Point<T, VSL_NAMESPACE::D1>((T)x);
	}

	template<typename R>
	constexpr auto _Point<R, VSL_NAMESPACE::D1>::length() const
	{
		return x;
	}

	// ---------------------------------------------------------------------------------------------------------------------

	template<typename R>
	constexpr bool _Point<R, VSL_NAMESPACE::D2>::operator<(const _Point<R, VSL_NAMESPACE::D2>& p) const
	{
		return length() < p.length();
	}

	template<typename R>
	constexpr _Point<R, VSL_NAMESPACE::D2> _Point<R, VSL_NAMESPACE::D2>::operator +() const {
		return *this;
	}

	template<typename R>
	constexpr _Point<R, VSL_NAMESPACE::D2> _Point<R, VSL_NAMESPACE::D2>::operator -() const {
		return _Point<R, VSL_NAMESPACE::D2>(-x, -y);
	}

	template<typename R>
	template<subtract_as<R> T>
	constexpr auto _Point<R, VSL_NAMESPACE::D2>::operator -(const _Point<T, VSL_NAMESPACE::D2>& p) const {
		return _Vector<decltype(x - p.x), VSL_NAMESPACE::D2>(x - p.x, y - p.y);
	}

	template<typename R>
	template<add_as<R> T>
	constexpr auto _Point<R, VSL_NAMESPACE::D2>::operator +(const _Vector<T, VSL_NAMESPACE::D2>& p) const {
		return _Point<decltype(x + p.x), VSL_NAMESPACE::D2>(x + p.x, y + p.y);
	}

	template<typename R>
	template<subtract_as<R> T>
	constexpr auto _Point<R, VSL_NAMESPACE::D2>::operator -(const _Vector<T, VSL_NAMESPACE::D2>& p) const {
		return _Point<decltype(x - p.x), VSL_NAMESPACE::D2>(x - p.x, y - p.y);
	}

	template<typename R>
	template<multiply_as<R> T>
	constexpr auto _Point<R, VSL_NAMESPACE::D2>::operator *(const T& m) const {
		return _Point<decltype(x* m), VSL_NAMESPACE::D2>(x * m, y * m);
	}

	template<typename R>
	template<division_as<R> T>
	constexpr auto _Point<R, VSL_NAMESPACE::D2>::operator /(const T& m) const {
		return _Point<decltype(x / m), VSL_NAMESPACE::D2>(x / m, y / m);
	}

	template<typename R>
	template<add_as<R> T>
	constexpr _Point<R, VSL_NAMESPACE::D2>& _Point<R, VSL_NAMESPACE::D2>::operator +=(const _Vector<T, VSL_NAMESPACE::D2>& p) const {
		this->x += p.x;
		this->y += p.y;
		return *this;
	}

	template<typename R>
	template<subtract_as<R> T>
	constexpr _Point<R, VSL_NAMESPACE::D2>& _Point<R, VSL_NAMESPACE::D2>::operator -=(const _Vector<T, VSL_NAMESPACE::D2>& p) const {
		this->x -= p.x;
		this->y -= p.y;
		return *this;
	}

	template<typename R>
	template<multiply_as<R> T>
	constexpr _Point<R, VSL_NAMESPACE::D2>& _Point<R, VSL_NAMESPACE::D2>::operator *=(const T& m) const {
		this->x *= m;
		this->y *= m;
		return *this;
	}

	template<typename R>
	template<division_as<R> T>
	constexpr _Point<R, VSL_NAMESPACE::D2>& _Point<R, VSL_NAMESPACE::D2>::operator /=(const T& m) const {
		this->x /= m;
		this->y /= m;
		return *this;
	}

	template<typename R>
	template<cast_as<R> T>
	constexpr _Point<T, VSL_NAMESPACE::D2> _Point<R, VSL_NAMESPACE::D2>::cast() const
	{
		return _Point<T, VSL_NAMESPACE::D2>((T)x, (T)y);
	}


	template<typename R>
	constexpr auto _Point<R, VSL_NAMESPACE::D2>::length() const
	{
		return std::sqrt(x * x + y * y);
	}

	// ---------------------------------------------------------------------------------------------------------------------

	template<typename R>
	constexpr bool _Point<R, VSL_NAMESPACE::D3>::operator<(const _Point<R, VSL_NAMESPACE::D3>& p) const
	{
		return length() < p.length();
	}

	template<typename R>
	constexpr _Point<R, VSL_NAMESPACE::D3> _Point<R, VSL_NAMESPACE::D3>::operator +() const {
		return *this;
	}

	template<typename R>
	constexpr _Point<R, VSL_NAMESPACE::D3> _Point<R, VSL_NAMESPACE::D3>::operator -() const {
		return _Point<R, VSL_NAMESPACE::D3>(-x, -y, -z);
	}

	template<typename R>
	template<subtract_as<R> T>
	constexpr auto _Point<R, VSL_NAMESPACE::D3>::operator -(const _Point<T, VSL_NAMESPACE::D3>& p) const {
		return _Vector<decltype(x - p.x), VSL_NAMESPACE::D3>(x - p.x, y - p.y, z - p.z);
	}

	template<typename R>
	template<add_as<R> T>
	constexpr auto _Point<R, VSL_NAMESPACE::D3>::operator +(const _Vector<T, VSL_NAMESPACE::D3>& p) const {
		return _Point<decltype(x + p.x), VSL_NAMESPACE::D3>(x + p.x, y + p.y, z + p.z);
	}

	template<typename R>
	template<subtract_as<R> T>
	constexpr auto _Point<R, VSL_NAMESPACE::D3>::operator -(const _Vector<T, VSL_NAMESPACE::D3>& p) const {
		return _Point<decltype(x - p.x), VSL_NAMESPACE::D3>(x - p.x, y - p.y, z - p.z);
	}

	template<typename R>
	template<multiply_as<R> T>
	constexpr auto _Point<R, VSL_NAMESPACE::D3>::operator *(const T& m) const {
		return _Point<decltype(x* m), VSL_NAMESPACE::D3>(x * m, y * m, z * m);
	}

	template<typename R>
	template<division_as<R> T>
	constexpr auto _Point<R, VSL_NAMESPACE::D3>::operator /(const T& m) const {
		return _Point<decltype(x / m), VSL_NAMESPACE::D3>(x / m, y / m, z / m);
	}

	template<typename R>
	template<add_as<R> T>
	constexpr _Point<R, VSL_NAMESPACE::D3>& _Point<R, VSL_NAMESPACE::D3>::operator +=(const _Vector<T, VSL_NAMESPACE::D3>& p) const {
		this->x += p.x;
		this->y += p.y;
		this->z += p.z;
		return *this;
	}

	template<typename R>
	template<subtract_as<R> T>
	constexpr _Point<R, VSL_NAMESPACE::D3>& _Point<R, VSL_NAMESPACE::D3>::operator -=(const _Vector<T, VSL_NAMESPACE::D3>& p) const {
		this->x -= p.x;
		this->y -= p.y;
		this->z -= p.z;
		return *this;
	}

	template<typename R>
	template<multiply_as<R> T>
	constexpr _Point<R, VSL_NAMESPACE::D3>& _Point<R, VSL_NAMESPACE::D3>::operator *=(const T& m) const {
		this->x *= m;
		this->y *= m;
		this->z *= m;
		return *this;
	}

	template<typename R>
	template<division_as<R> T>
	constexpr _Point<R, VSL_NAMESPACE::D3>& _Point<R, VSL_NAMESPACE::D3>::operator /=(const T& m) const {
		this->x /= m;
		this->y /= m;
		this->z /= m;
		return *this;
	}

	template<typename R>
	template<cast_as<R> T>
	constexpr _Point<T, VSL_NAMESPACE::D3> _Point<R, VSL_NAMESPACE::D3>::cast() const
	{
		return _Point<T, VSL_NAMESPACE::D3>((T)x, (T)y);
	}


	template<typename R>
	constexpr auto _Point<R, VSL_NAMESPACE::D3>::length() const
	{
		return std::sqrt(x * x + y * y + z * z);
	}
}