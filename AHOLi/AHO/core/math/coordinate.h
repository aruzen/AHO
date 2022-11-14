#pragma once
#include "../../../VSL/dimention.h"

#include "../../define.h"

#include "../../concepts.h"

namespace AHO_NAMESPACE {
	namespace coordinate {
		template <typename T>
		struct X {
			T value;

			constexpr X(const T& t) : value(t) {};
			constexpr X() {};

			constexpr X<T> operator +() { static_assert(plusable<T>); return { value }; };
			constexpr X<T> operator -() { static_assert(minusable<T>); return { -value }; };
			template <add_as<T> R>
			constexpr X<decltype(std::declval<T>() + std::declval<R>())> operator +(X<R> t) { return { value + t.value }; };
			template <subtract_as<T> R>
			constexpr X<decltype(std::declval<T>() - std::declval<R>())> operator -(X<R> t) { return { value - t.value }; };
			template <multiply_as<T> R>
			constexpr X<decltype(std::declval<T>()* std::declval<R>())> operator *(X<R> t) { return { value * t.value }; };
			template <division_as<T> R>
			constexpr X<decltype(std::declval<T>() / std::declval<R>())> operator /(X<R> t) { return { value / t.value }; };

			template <add_as<T> R>
			constexpr X<T>& operator +=(X<R> t) { value += t.value; return *this; };
			template <subtract_as<T> R>
			constexpr X<T>& operator -=(X<R> t) { value -= t.value; return *this; };
			template <multiply_as<T> R>
			constexpr X<T>& operator *=(X<R> t) { value *= t.value; return *this; };
			template <division_as<T> R>
			constexpr X<T>& operator /=(X<R> t) { value /= t.value; return *this; };

			template <cast_as<T> R>
			X<R> cast() const { return { (R)value }; }
		};

		template <typename T>
		struct Y {
			T value;

			constexpr Y(const T& t) : value(t) {};
			constexpr Y() {};

			constexpr Y<T> operator +() { static_assert(plusable<T>); return { value }; };
			constexpr Y<T> operator -() { static_assert(minusable<T>); return { -value }; };
			template <add_as<T> R>
			constexpr Y<decltype(std::declval<T>() + std::declval<R>())> operator +(Y<R> t) { return { value + t.value }; };
			template <subtract_as<T> R>
			constexpr Y<decltype(std::declval<T>() - std::declval<R>())> operator -(Y<R> t) { return { value - t.value }; };
			template <multiply_as<T> R>
			constexpr Y<decltype(std::declval<T>()* std::declval<R>())> operator *(Y<R> t) { return { value * t.value }; };
			template <division_as<T> R>
			constexpr Y<decltype(std::declval<T>() / std::declval<R>())> operator /(Y<R> t) { return { value / t.value }; };

			template <add_as<T> R>
			constexpr Y<T>& operator +=(Y<R> t) { value += t.value; return *this; };
			template <subtract_as<T> R>
			constexpr Y<T>& operator -=(Y<R> t) { value -= t.value; return *this; };
			template <multiply_as<T> R>
			constexpr Y<T>& operator *=(Y<R> t) { value *= t.value; return *this; };
			template <division_as<T> R>
			constexpr Y<T>& operator /=(Y<R> t) { value /= t.value; return *this; };

			template <cast_as<T> R>
			Y<R> cast() const { return { (R)value }; }
		};

		template <typename T>
		struct Z {
			T value;

			constexpr Z(const T& t) : value(t) {};
			constexpr Z() {};

			constexpr Z<T> operator +() { static_assert(plusable<T>); return { value }; };
			constexpr Z<T> operator -() { static_assert(minusable<T>); return { -value }; };
			template <add_as<T> R>
			constexpr Z<decltype(std::declval<T>() + std::declval<R>())> operator +(Z<R> t) { return { value + t.value }; };
			template <subtract_as<T> R>
			constexpr Z<decltype(std::declval<T>() - std::declval<R>())> operator -(Z<R> t) { return { value - t.value }; };
			template <multiply_as<T> R>
			constexpr Z<decltype(std::declval<T>()* std::declval<R>())> operator *(Z<R> t) { return { value * t.value }; };
			template <division_as<T> R>
			constexpr Z<decltype(std::declval<T>() / std::declval<R>())> operator /(Z<R> t) { return { value / t.value }; };

			template <add_as<T> R>
			constexpr Z<T>& operator +=(Z<R> t) { value += t.value; return *this; };
			template <subtract_as<T> R>
			constexpr Z<T>& operator -=(Z<R> t) { value -= t.value; return *this; };
			template <multiply_as<T> R>
			constexpr Z<T>& operator *=(Z<R> t) { value *= t.value; return *this; };
			template <division_as<T> R>
			constexpr Z<T>& operator /=(Z<R> t) { value /= t.value; return *this; };

			template <cast_as<T> R>
			Z<R> cast() const { return { (R)value }; }
		};

		constexpr X<double> operator"" AHO_LITERAL(x)(long double v) { return { (double)v }; };
		constexpr X<int> operator"" AHO_LITERAL(x)(size_t v) { return { (int)v }; };
		constexpr X<float> operator"" AHO_LITERAL(f_x)(long double v) { return { (float)v }; };
		constexpr X<size_t> operator"" AHO_LITERAL(l_x)(size_t v) { return { v }; };
		constexpr X<double> x(1.0);

		constexpr Y<double> operator"" AHO_LITERAL(y)(long double v) { return { (double)v }; };
		constexpr Y<int> operator"" AHO_LITERAL(y)(size_t v) { return { (int)v }; };
		constexpr Y<float> operator"" AHO_LITERAL(f_y)(long double v) { return { (float)v }; };
		constexpr Y<size_t> operator"" AHO_LITERAL(l_y)(size_t v) { return { v }; };
		constexpr Y<double> y(1.0);

		constexpr Z<double> operator"" AHO_LITERAL(z)(long double v) { return { (double)v }; };
		constexpr Z<int> operator"" AHO_LITERAL(z)(size_t v) { return { (int)v }; };
		constexpr Z<float> operator"" AHO_LITERAL(f_z)(long double v) { return { (float)v }; };
		constexpr Z<size_t> operator"" AHO_LITERAL(l_z)(size_t v) { return { v }; };
		constexpr Z<double> z(1.0);

		template <bool ...Booleans>
		constexpr size_t _CountTruesFunc() {
			size_t n = 0;
			bool bools[] = { Booleans... };
			for (bool f : bools)
				if (f)
					n++;
			return n;
		}

		template <bool ...Booleans>
		struct _CountTrues {
			constexpr static size_t value = _CountTruesFunc<Booleans...>();
		};

		template <VSL_NAMESPACE::is_dimention D, bool EnableX, bool EnableY = false, bool EnableZ = false>
		struct _CoodinateInfo {
			using dimention = D;
			constexpr static bool EnabledX = EnableX;
			constexpr static bool EnabledY = EnableY;
			constexpr static bool EnabledZ = EnableZ;
			constexpr static bool IndexX = _CountTruesFunc<EnableX>();
			constexpr static bool IndexY = _CountTruesFunc<EnableX, EnableY>();
			constexpr static bool IndexZ = _CountTruesFunc<EnableX, EnableY, EnableZ>();
		};

		template<bool EnableX = false, bool EnableY = false, bool EnableZ = false>
		struct _MakeCoodinateInfo {
			using value = _CoodinateInfo<typename VSL_NAMESPACE::Dimention<_CountTrues<EnableX, EnableY, EnableZ>::value>::value, EnableX, EnableY, EnableZ>;
		};

		template<VSL_NAMESPACE::is_dimention D>
		struct _DefaultCoodinateInfo {};

		template<>
		struct _DefaultCoodinateInfo<VSL_NAMESPACE::D1> {
			using value = _CoodinateInfo<VSL_NAMESPACE::D1, true>;
		};

		template<>
		struct _DefaultCoodinateInfo<VSL_NAMESPACE::D2> {
			using value = _CoodinateInfo<VSL_NAMESPACE::D2, true, true>;
		};

		template<>
		struct _DefaultCoodinateInfo<VSL_NAMESPACE::D3> {
			using value = _CoodinateInfo<VSL_NAMESPACE::D3, true, true, true>;
		};

		template <typename ElementType, typename _CoordinateInfo>
		struct _CoordinateSet {};

#define AHO_DEFINE_SPECIAL_COORDINATE_SET1(Upper, Lower) \
		template <typename ElementType> \
		struct _CoordinateSet<ElementType, typename _MakeCoodinateInfo< \
							      ::std::same_as<X<ElementType>, Upper<ElementType>>, \
								  ::std::same_as<Y<ElementType>, Upper<ElementType>>, \
								  ::std::same_as<Z<ElementType>, Upper<ElementType>>  \
							  >::value> { \
			using coordinate_info = typename _MakeCoodinateInfo< \
								  ::std::same_as<X<ElementType>, Upper<ElementType>>, \
								  ::std::same_as<Y<ElementType>, Upper<ElementType>>, \
								  ::std::same_as<Z<ElementType>, Upper<ElementType>>  \
							  >::value; \
			union { Upper<ElementType> Lower, c1; }; \
		};

#define AHO_DEFINE_SPECIAL_COORDINATE_SET2(Upper1, Upper2, Lower1, Lower2) \
		template <typename ElementType> \
		struct _CoordinateSet<ElementType, typename _MakeCoodinateInfo< \
							      (::std::same_as<X<ElementType>, Upper1<ElementType>> || ::std::same_as<X<ElementType>, Upper2<ElementType>>), \
							      (::std::same_as<Y<ElementType>, Upper1<ElementType>> || ::std::same_as<Y<ElementType>, Upper2<ElementType>>), \
							      (::std::same_as<Z<ElementType>, Upper1<ElementType>> || ::std::same_as<Z<ElementType>, Upper2<ElementType>>)  \
							  >::value> { \
			using coordinate_info = typename _MakeCoodinateInfo< \
							      (::std::same_as<X<ElementType>, Upper1<ElementType>> || ::std::same_as<X<ElementType>, Upper2<ElementType>>), \
							      (::std::same_as<Y<ElementType>, Upper1<ElementType>> || ::std::same_as<Y<ElementType>, Upper2<ElementType>>), \
							      (::std::same_as<Z<ElementType>, Upper1<ElementType>> || ::std::same_as<Z<ElementType>, Upper2<ElementType>>)  \
							  >::value; \
			union { Upper1<ElementType> Lower1, c1; }; \
			union { Upper2<ElementType> Lower2, c2; }; \
		};

		AHO_DEFINE_SPECIAL_COORDINATE_SET1(X, x);
		AHO_DEFINE_SPECIAL_COORDINATE_SET1(Y, y);
		AHO_DEFINE_SPECIAL_COORDINATE_SET1(Z, z);

		AHO_DEFINE_SPECIAL_COORDINATE_SET2(X, Y, x, y);
		AHO_DEFINE_SPECIAL_COORDINATE_SET2(X, Z, x, z);
		AHO_DEFINE_SPECIAL_COORDINATE_SET2(Y, Z, y, z);

		template <typename ElementType>
		struct _CoordinateSet<ElementType, _MakeCoodinateInfo<true, true, true>::value> {
			using coordinate_info = _MakeCoodinateInfo<true, true, true>::value;
			union { X<ElementType> x, c1; };
			union { Y<ElementType> y, c2; };
			union { Z<ElementType> z, c3; };
		};

		using coordinate::X;
		using coordinate::Y;
		using coordinate::Z;
	}