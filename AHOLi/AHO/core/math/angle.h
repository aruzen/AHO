#pragma once
#include "../../define.h"

#include <numbers>

namespace AHO_NAMESPACE {
	struct degree;
	struct radian {
		long double value;

		constexpr auto operator <=>(const radian& r) const = default;

		constexpr radian operator +() const { return { value }; }
		constexpr radian operator -() const { return { -value }; }
		constexpr radian operator +(const radian& r) const { return { value + r.value}; }
		constexpr radian operator -(const radian& r) const { return { value - r.value }; }
		constexpr radian operator *(const double& m) const { return { value * m }; }
		constexpr radian operator *(const size_t& m) const { return { value * m }; }
		constexpr radian operator /(const double& m) const { return { value / m }; }
		constexpr radian operator /(const size_t& m) const { return { value / m }; }

		constexpr radian& operator +=(radian r) { value += r.value; return *this; }
		constexpr radian& operator -=(radian r) { value -= r.value; return *this; }
		constexpr radian& operator *=(double m) { value *= m; return *this; }
		constexpr radian& operator *=(size_t m) { value *= m; return *this; }
		constexpr radian& operator /=(double m) { value /= m; return *this; }
		constexpr radian& operator /=(size_t m) { value /= m; return *this; }

		constexpr operator double() const { return (double)value; }
		constexpr operator long double() const { return value; }
		constexpr explicit operator float() const { return (float)value; }
		constexpr explicit operator int() const { return (int)value; }
		constexpr explicit operator size_t() const { return (size_t)value; }

		constexpr operator degree() const;
	};

	struct degree {
		long double value;

		constexpr auto operator <=>(const degree& r) const = default;

		constexpr degree operator +() const { return { value }; }
		constexpr degree operator -() const { return { -value }; }
		constexpr degree operator +(const degree& r) const { return { value + r.value }; }
		constexpr degree operator -(const degree& r) const { return { value - r.value }; }
		constexpr degree operator *(const double& m) const { return { value * m }; }
		constexpr degree operator *(const size_t& m) const { return { value * m }; }
		constexpr degree operator /(const double& m) const { return { value / m }; }
		constexpr degree operator /(const size_t& m) const { return { value / m }; }

		constexpr degree& operator +=(degree r) { value += r.value; return *this; }
		constexpr degree& operator -=(degree r) { value -= r.value; return *this; }
		constexpr degree& operator *=(double m) { value *= m; return *this; }
		constexpr degree& operator *=(size_t m) { value *= m; return *this; }
		constexpr degree& operator /=(double m) { value /= m; return *this; }
		constexpr degree& operator /=(size_t m) { value /= m; return *this; }

		constexpr operator double() const { return (double)value; }
		constexpr operator long double() const { return value; }
		constexpr explicit operator float() const { return (float)value; }
		constexpr explicit operator int() const { return (int)value; }
		constexpr explicit operator size_t() const { return (size_t)value; }

		operator radian() const;
	};

	constexpr radian::operator AHO_NAMESPACE::degree() const {
		return { value * 180.0 / std::numbers::pi };
	}

	constexpr degree::operator AHO_NAMESPACE::radian() const {
		return { value / 180.0 * std::numbers::pi };
	}

	constexpr radian pi = { std::numbers::pi };

	constexpr radian operator"" AHO_LITERAL(rad)(long double d) {
		return { d };
	}

	constexpr radian operator"" AHO_LITERAL(pi)(long double d) {
		return { d * std::numbers::pi };
	}

	constexpr radian operator"" AHO_LITERAL(\u03c0)(long double d) {
		return { d * std::numbers::pi };
	}

	constexpr degree operator"" AHO_LITERAL(deg)(long double d) {
		return { d };
	}
}