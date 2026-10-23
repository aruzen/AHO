#pragma once
#include "../../define.hpp"
#include <compare>
#include <cstddef>
#include <ostream>

#include <numbers>
#include <type_traits>

namespace AHO_NAMESPACE {
    namespace angle {
        struct Degree;
        struct Radian {
            long double value;

            constexpr auto operator<=>(const Radian& r) const = default;

            constexpr Radian operator+() const { return {value}; }
            constexpr Radian operator-() const { return {-value}; }

            constexpr Radian& operator+=(Radian r) {
                value += r.value;
                return *this;
            }
            constexpr Radian& operator-=(Radian r) {
                value -= r.value;
                return *this;
            }
            constexpr Radian& operator*=(double m) {
                value *= m;
                return *this;
            }
            constexpr Radian& operator*=(size_t m) {
                value *= m;
                return *this;
            }
            constexpr Radian& operator/=(double m) {
                value /= m;
                return *this;
            }
            constexpr Radian& operator/=(size_t m) {
                value /= m;
                return *this;
            }

            template<typename Scalar>
            requires std::is_arithmetic_v<Scalar>
            constexpr Radian& operator*=(const Scalar& m) {
                value *= m;
                return *this;
            }

            template<typename Scalar>
            requires std::is_arithmetic_v<Scalar>
            constexpr Radian& operator/=(const Scalar& m) {
                value /= m;
                return *this;
            }

            constexpr operator double() const { return (double)value; }
            constexpr operator long double() const { return value; }
            constexpr explicit operator float() const { return (float)value; }
            constexpr explicit operator int() const { return (int)value; }
            constexpr explicit operator size_t() const { return (size_t)value; }

            constexpr operator Degree() const;
        };

        struct Degree {
            long double value;

            constexpr auto operator<=>(const Degree& r) const = default;

            constexpr Degree operator+() const { return {value}; }
            constexpr Degree operator-() const { return {-value}; }

            constexpr Degree& operator+=(Degree r) {
                value += r.value;
                return *this;
            }
            constexpr Degree& operator-=(Degree r) {
                value -= r.value;
                return *this;
            }
            constexpr Degree& operator*=(double m) {
                value *= m;
                return *this;
            }
            constexpr Degree& operator*=(size_t m) {
                value *= m;
                return *this;
            }
            constexpr Degree& operator/=(double m) {
                value /= m;
                return *this;
            }
            constexpr Degree& operator/=(size_t m) {
                value /= m;
                return *this;
            }

            template<typename Scalar>
            requires std::is_arithmetic_v<Scalar>
            constexpr Degree& operator*=(const Scalar& m) {
                value *= m;
                return *this;
            }

            template<typename Scalar>
            requires std::is_arithmetic_v<Scalar>
            constexpr Degree& operator/=(const Scalar& m) {
                value /= m;
                return *this;
            }

            constexpr operator double() const { return (double)value; }
            constexpr operator long double() const { return value; }
            constexpr explicit operator float() const { return (float)value; }
            constexpr explicit operator int() const { return (int)value; }
            constexpr explicit operator size_t() const { return (size_t)value; }

            constexpr operator Radian() const;
        };

        constexpr Radian operator+(const Radian& o, const Radian& r) {
            return {o.value + r.value};
        }
        constexpr Radian operator-(const Radian& o, const Radian& r) {
            return {o.value - r.value};
        }
        constexpr Radian operator*(const Radian& o, const double& m) {
            return {o.value * m};
        }
        constexpr Radian operator*(const Radian& o, const size_t& m) {
            return {o.value * m};
        }
        constexpr Radian operator*(const Radian& o, const float& m) {
            return {o.value * m};
        }
        constexpr Radian operator/(const Radian& o, const double& m) {
            return {o.value / m};
        }
        constexpr Radian operator/(const Radian& o, const size_t& m) {
            return {o.value / m};
        }
        constexpr Radian operator/(const Radian& o, const float& m) {
            return {o.value / m};
        }
        constexpr Radian operator*(const double& m, const Radian& o) {
            return {m * o.value};
        }
        constexpr Radian operator*(const size_t& m, const Radian& o) {
            return {m * o.value};
        }
        constexpr Radian operator*(const float& m, const Radian& o) {
            return {m * o.value};
        }
        constexpr Radian operator/(const double& m, const Radian& o) {
            return {m / o.value};
        }
        constexpr Radian operator/(const size_t& m, const Radian& o) {
            return {m / o.value};
        }
        constexpr Radian operator/(const float& m, const Radian& o) {
            return {m / o.value};
        }

        constexpr Degree operator+(const Degree& o, const Degree& r) {
            return {o.value + r.value};
        }
        constexpr Degree operator-(const Degree& o, const Degree& r) {
            return {o.value - r.value};
        }
        constexpr Degree operator*(const Degree& o, const double& m) {
            return {o.value * m};
        }
        constexpr Degree operator*(const Degree& o, const size_t& m) {
            return {o.value * m};
        }
        constexpr Degree operator*(const Degree& o, const float& m) {
            return {o.value * m};
        }
        constexpr Degree operator/(const Degree& o, const double& m) {
            return {o.value / m};
        }
        constexpr Degree operator/(const Degree& o, const size_t& m) {
            return {o.value / m};
        }
        constexpr Degree operator/(const Degree& o, const float& m) {
            return {o.value / m};
        }
        constexpr Degree operator*(const double& m, const Degree& o) {
            return {m * o.value};
        }
        constexpr Degree operator*(const size_t& m, const Degree& o) {
            return {m * o.value};
        }
        constexpr Degree operator*(const float& m, const Degree& o) {
            return {m * o.value};
        }
        constexpr Degree operator/(const double& m, const Degree& o) {
            return {m / o.value};
        }
        constexpr Degree operator/(const size_t& m, const Degree& o) {
            return {m / o.value};
        }
        constexpr Degree operator/(const float& m, const Degree& o) {
            return {m / o.value};
        }

        template<typename Scalar>
        requires std::is_arithmetic_v<Scalar>
        constexpr Radian operator*(const Radian& o, const Scalar& m) {
            return {o.value * m};
        }
        template<typename Scalar>
        requires std::is_arithmetic_v<Scalar>
        constexpr Radian operator*(const Scalar& m, const Radian& o) {
            return {m * o.value};
        }
        template<typename Scalar>
        requires std::is_arithmetic_v<Scalar>
        constexpr Radian operator/(const Radian& o, const Scalar& m) {
            return {o.value / m};
        }
        template<typename Scalar>
        requires std::is_arithmetic_v<Scalar>
        constexpr Radian operator/(const Scalar& m, const Radian& o) {
            return {m / o.value};
        }

        template<typename Scalar>
        requires std::is_arithmetic_v<Scalar>
        constexpr Degree operator*(const Degree& o, const Scalar& m) {
            return {o.value * m};
        }
        template<typename Scalar>
        requires std::is_arithmetic_v<Scalar>
        constexpr Degree operator*(const Scalar& m, const Degree& o) {
            return {m * o.value};
        }
        template<typename Scalar>
        requires std::is_arithmetic_v<Scalar>
        constexpr Degree operator/(const Degree& o, const Scalar& m) {
            return {o.value / m};
        }
        template<typename Scalar>
        requires std::is_arithmetic_v<Scalar>
        constexpr Degree operator/(const Scalar& m, const Degree& o) {
            return {m / o.value};
        }

        namespace literals {
            // Preserve the precision of the long double value, including on older standard libraries.
            constexpr Radian PI = {3.141592653589793238462643383279502884L};

            constexpr Radian operator AHO_LITERAL(rad)(long double d) {
                return {d};
            }

            constexpr Radian operator AHO_LITERAL(pi)(long double d) {
                return {d * PI.value};
            }

            constexpr Radian operator AHO_LITERAL(\u03c0)(long double d) {
                return {d * PI.value};
            }

            constexpr Degree operator AHO_LITERAL(deg)(long double d) {
                return {d};
            }
        } // namespace literals

        constexpr Radian::operator Degree() const {
            return {value * 180.0 / literals::PI.value};
        }

        constexpr Degree::operator Radian() const {
            return {value / 180.0 * literals::PI.value};
        }

    } // namespace angle

    using angle::Radian;
    using angle::Degree;

    namespace literals {
        using namespace angle::literals;
    }
} // namespace AHO_NAMESPACE
