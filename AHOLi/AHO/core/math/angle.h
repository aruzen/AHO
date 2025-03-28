#pragma once
#include "../../define.h"

#include <numbers>

namespace AHO_NAMESPACE {
	namespace angle {
		struct Degree;
		struct Radian {
			long double value;

			constexpr auto operator <=>(const Radian& r) const = default;

			constexpr Radian operator +() const { return { value }; }
			constexpr Radian operator -() const { return { -value }; }
			constexpr Radian operator +(const Radian& r) const { return { value + r.value }; }
			constexpr Radian operator -(const Radian& r) const { return { value - r.value }; }
			constexpr Radian operator *(const double& m) const { return { value * m }; }
			constexpr Radian operator *(const size_t& m) const { return { value * m }; }
			constexpr Radian operator /(const double& m) const { return { value / m }; }
			constexpr Radian operator /(const size_t& m) const { return { value / m }; }

			constexpr Radian& operator +=(Radian r) { value += r.value; return *this; }
			constexpr Radian& operator -=(Radian r) { value -= r.value; return *this; }
			constexpr Radian& operator *=(double m) { value *= m; return *this; }
			constexpr Radian& operator *=(size_t m) { value *= m; return *this; }
			constexpr Radian& operator /=(double m) { value /= m; return *this; }
			constexpr Radian& operator /=(size_t m) { value /= m; return *this; }

			constexpr operator double() const { return (double)value; }
			constexpr operator long double() const { return value; }
			constexpr explicit operator float() const { return (float)value; }
			constexpr explicit operator int() const { return (int)value; }
			constexpr explicit operator size_t() const { return (size_t)value; }

			constexpr operator Degree() const;
		};

		struct Degree {
			long double value;

			constexpr auto operator <=>(const Degree& r) const = default;

			constexpr Degree operator +() const { return { value }; }
			constexpr Degree operator -() const { return { -value }; }
			constexpr Degree operator +(const Degree& r) const { return { value + r.value }; }
			constexpr Degree operator -(const Degree& r) const { return { value - r.value }; }
			constexpr Degree operator *(const double& m) const { return { value * m }; }
			constexpr Degree operator *(const size_t& m) const { return { value * m }; }
			constexpr Degree operator /(const double& m) const { return { value / m }; }
			constexpr Degree operator /(const size_t& m) const { return { value / m }; }

			constexpr Degree& operator +=(Degree r) { value += r.value; return *this; }
			constexpr Degree& operator -=(Degree r) { value -= r.value; return *this; }
			constexpr Degree& operator *=(double m) { value *= m; return *this; }
			constexpr Degree& operator *=(size_t m) { value *= m; return *this; }
			constexpr Degree& operator /=(double m) { value /= m; return *this; }
			constexpr Degree& operator /=(size_t m) { value /= m; return *this; }

			constexpr operator double() const { return (double)value; }
			constexpr operator long double() const { return value; }
			constexpr explicit operator float() const { return (float)value; }
			constexpr explicit operator int() const { return (int)value; }
			constexpr explicit operator size_t() const { return (size_t)value; }

			constexpr operator Radian() const;
		};

		constexpr Radian::operator Degree() const {
			return { value * 180.0 / std::numbers::pi };
		}

		constexpr Degree::operator Radian() const {
			return { value / 180.0 * std::numbers::pi };
		}

		namespace literals {
			constexpr Radian pi = { std::numbers::pi };

			constexpr Radian operator"" AHO_LITERAL(rad)(long double d) {
				return { d };
			}

			constexpr Radian operator"" AHO_LITERAL(pi)(long double d) {
				return { d * std::numbers::pi };
			}

			constexpr Radian operator"" AHO_LITERAL(\u03c0)(long double d) {
				return { d * std::numbers::pi };
			}

			constexpr Degree operator"" AHO_LITERAL(deg)(long double d) {
				return { d };
			}
		}
	}

	using angle::Radian;
	using angle::Degree;

	namespace literals {
		using namespace angle::literals;
	}
}