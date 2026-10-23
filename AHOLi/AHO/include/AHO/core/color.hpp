#pragma once
#pragma once
#include "../define.hpp"
#include "../concepts.hpp"
#include "VSL/format.hpp"
#include <cmath>
#include <limits>

#include <iostream>
#include <cstdint>
#include <algorithm>

namespace AHO_NAMESPACE {
    namespace color {
        template<typename T>
        struct RGB;
        template<typename T>
        struct RGBA;
        template<typename H, typename SV = H>
        struct HSV;
        template<typename H, typename SV = H>
        struct HSVA;

#define RGB_ELEMENT_TYPE_DEFINITION_EXPANDER(Upper, Lower)                                       \
    template<typename T>                                                                         \
    struct Upper {                                                                               \
        using ElementType = T;                                                                   \
        constexpr static bool IS_RGB_ELEMENET = true;                                            \
                                                                                                 \
        T value;                                                                                 \
                                                                                                 \
        constexpr static char UPPER_CHAR = #Upper[0];                                            \
        constexpr static char LOWER_CHAR = #Lower[0];                                            \
                                                                                                 \
        constexpr Upper(const T& t) : value(t) {};                                               \
        constexpr Upper() : value{} {};                                                          \
                                                                                                 \
        constexpr Upper<T> operator+() const {                                                   \
            static_assert(concepts::plusable<T>);                                                \
            return {value};                                                                      \
        };                                                                                       \
        constexpr Upper<T> operator-() const {                                                   \
            static_assert(concepts::minusable<T>);                                               \
            return Upper<T>(-value);                                                             \
        };                                                                                       \
        template<typename RV>                                                                    \
        requires concepts::add_as<const T&, const RV&>                                           \
        constexpr auto operator+(const Upper<RV>& t) const -> Upper<decltype(value + t.value)> { \
            return {value + t.value};                                                            \
        };                                                                                       \
        template<typename RV>                                                                    \
        requires concepts::subtract_as<const T&, const RV&>                                      \
        constexpr auto operator-(const Upper<RV>& t) const -> Upper<decltype(value - t.value)> { \
            return {value - t.value};                                                            \
        };                                                                                       \
        template<typename RV>                                                                    \
        requires concepts::multiply_as<const T&, const RV&>                                      \
        constexpr auto operator*(const Upper<RV>& t) const -> Upper<decltype(value * t.value)> { \
            return {value * t.value};                                                            \
        };                                                                                       \
        template<typename RV>                                                                    \
        requires concepts::division_as<const T&, const RV&>                                      \
        constexpr auto operator/(const Upper<RV>& t) const -> Upper<decltype(value / t.value)> { \
            return {value / t.value};                                                            \
        };                                                                                       \
                                                                                                 \
        template<typename RV>                                                                    \
        requires concepts::add_assign_as<T&, const RV&>                                          \
        constexpr Upper<T>& operator+=(const Upper<RV>& t) {                                     \
            value += t.value;                                                                    \
            return *this;                                                                        \
        };                                                                                       \
        template<typename RV>                                                                    \
        requires concepts::subtract_assign_as<T&, const RV&>                                     \
        constexpr Upper<T>& operator-=(const Upper<RV>& t) {                                     \
            value -= t.value;                                                                    \
            return *this;                                                                        \
        };                                                                                       \
        template<typename RV>                                                                    \
        requires concepts::multiply_assign_as<T&, const RV&>                                     \
        constexpr Upper<T>& operator*=(const Upper<RV>& t) {                                     \
            value *= t.value;                                                                    \
            return *this;                                                                        \
        };                                                                                       \
        template<typename RV>                                                                    \
        requires concepts::division_assign_as<T&, const RV&>                                     \
        constexpr Upper<T>& operator/=(const Upper<RV>& t) {                                     \
            value /= t.value;                                                                    \
            return *this;                                                                        \
        };                                                                                       \
                                                                                                 \
        template<typename RV>                                                                    \
        requires concepts::cast_as<T, RV>                                                        \
        Upper<RV> cast() const {                                                                 \
            return {(RV)value};                                                                  \
        }                                                                                        \
    };

#define HSV_ELEMENT_TYPE_DEFINITION_EXPANDER(Upper, Lower)                                       \
    template<typename T>                                                                         \
    struct Upper {                                                                               \
        using ElementType = T;                                                                   \
        constexpr static bool IS_HSV_ELEMENET = true;                                            \
                                                                                                 \
        T value;                                                                                 \
                                                                                                 \
        constexpr static char UPPER_CHAR = #Upper[0];                                            \
        constexpr static char LOWER_CHAR = #Lower[0];                                            \
                                                                                                 \
        constexpr Upper(const T& t) : value(t) {};                                               \
        constexpr Upper() : value{} {};                                                          \
                                                                                                 \
        constexpr Upper<T> operator+() const {                                                   \
            static_assert(concepts::plusable<T>);                                                \
            return {value};                                                                      \
        };                                                                                       \
        constexpr Upper<T> operator-() const {                                                   \
            static_assert(concepts::minusable<T>);                                               \
            return Upper<T>(-value);                                                             \
        };                                                                                       \
        template<typename RV>                                                                    \
        requires concepts::add_as<const T&, const RV&>                                           \
        constexpr auto operator+(const Upper<RV>& t) const -> Upper<decltype(value + t.value)> { \
            return {value + t.value};                                                            \
        };                                                                                       \
        template<typename RV>                                                                    \
        requires concepts::subtract_as<const T&, const RV&>                                      \
        constexpr auto operator-(const Upper<RV>& t) const -> Upper<decltype(value - t.value)> { \
            return {value - t.value};                                                            \
        };                                                                                       \
        template<typename RV>                                                                    \
        requires concepts::multiply_as<const T&, const RV&>                                      \
        constexpr auto operator*(const Upper<RV>& t) const -> Upper<decltype(value * t.value)> { \
            return {value * t.value};                                                            \
        };                                                                                       \
        template<typename RV>                                                                    \
        requires concepts::division_as<const T&, const RV&>                                      \
        constexpr auto operator/(const Upper<RV>& t) const -> Upper<decltype(value / t.value)> { \
            return {value / t.value};                                                            \
        };                                                                                       \
                                                                                                 \
        template<typename RV>                                                                    \
        requires concepts::add_assign_as<T&, const RV&>                                          \
        constexpr Upper<T>& operator+=(const Upper<RV>& t) {                                     \
            value += t.value;                                                                    \
            return *this;                                                                        \
        };                                                                                       \
        template<typename RV>                                                                    \
        requires concepts::subtract_assign_as<T&, const RV&>                                     \
        constexpr Upper<T>& operator-=(const Upper<RV>& t) {                                     \
            value -= t.value;                                                                    \
            return *this;                                                                        \
        };                                                                                       \
        template<typename RV>                                                                    \
        requires concepts::multiply_assign_as<T&, const RV&>                                     \
        constexpr Upper<T>& operator*=(const Upper<RV>& t) {                                     \
            value *= t.value;                                                                    \
            return *this;                                                                        \
        };                                                                                       \
        template<typename RV>                                                                    \
        requires concepts::division_assign_as<T&, const RV&>                                     \
        constexpr Upper<T>& operator/=(const Upper<RV>& t) {                                     \
            value /= t.value;                                                                    \
            return *this;                                                                        \
        };                                                                                       \
                                                                                                 \
        template<typename RV>                                                                    \
        requires concepts::cast_as<T, RV>                                                        \
        Upper<RV> cast() const {                                                                 \
            return {(RV)value};                                                                  \
        }                                                                                        \
    };

        template<typename T>
        struct A {
            using ElementType = T;

            T value;

            constexpr static char UPPER_CHAR = 'A';
            constexpr static char LOWER_CHAR = 'a';

            constexpr A(const T& t) : value(t) {};
            constexpr A() : value{} {};

            constexpr A<T> operator+() const {
                static_assert(concepts::plusable<T>);
                return {value};
            };
            constexpr A<T> operator-() const {
                static_assert(concepts::minusable<T>);
                return A<T>(-value);
            };
            template<typename R>
            requires concepts::add_as<const T&, R&>
            constexpr auto operator+(A<R> t) const -> A<decltype(value + t.value)> { return {value + t.value}; };
            template<typename R>
            requires concepts::subtract_as<const T&, R&>
            constexpr auto operator-(A<R> t) const -> A<decltype(value - t.value)> { return {value - t.value}; };
            template<typename R>
            requires concepts::multiply_as<const T&, R&>
            constexpr auto operator*(A<R> t) const -> A<decltype(value * t.value)> { return {value * t.value}; };
            template<typename R>
            requires concepts::division_as<const T&, R&>
            constexpr auto operator/(A<R> t) const -> A<decltype(value / t.value)> { return {value / t.value}; };

            template<typename R>
            requires concepts::add_assign_as<T&, R&>
            constexpr A<T>& operator+=(A<R> t) {
                value += t.value;
                return *this;
            };
            template<typename R>
            requires concepts::subtract_assign_as<T&, R&>
            constexpr A<T>& operator-=(A<R> t) {
                value -= t.value;
                return *this;
            };
            template<typename R>
            requires concepts::multiply_assign_as<T&, R&>
            constexpr A<T>& operator*=(A<R> t) {
                value *= t.value;
                return *this;
            };
            template<typename R>
            requires concepts::division_assign_as<T&, R&>
            constexpr A<T>& operator/=(A<R> t) {
                value /= t.value;
                return *this;
            };

            template<typename R>
            requires concepts::cast_as<T, R>
            A<R> cast() const { return {(R)value}; }
        };

        RGB_ELEMENT_TYPE_DEFINITION_EXPANDER(R, r);
        RGB_ELEMENT_TYPE_DEFINITION_EXPANDER(G, g);
        RGB_ELEMENT_TYPE_DEFINITION_EXPANDER(B, b);

        HSV_ELEMENT_TYPE_DEFINITION_EXPANDER(H, h);
        HSV_ELEMENT_TYPE_DEFINITION_EXPANDER(S, s);
        HSV_ELEMENT_TYPE_DEFINITION_EXPANDER(V, v);

        namespace concepts {
            template<typename T>
            concept IS_RGB_ELEMENET = T::IS_RGB_ELEMENET;

            template<typename T>
            concept IS_HSV_ELEMENET = T::IS_HSV_ELEMENET;
        } // namespace concepts

        template<typename ElementType_>
        struct RGB {
            using ElementType = ElementType_;
            static constexpr VSL_NAMESPACE::data_format::Format GRAPHIC_TYPE = VSL_NAMESPACE::data_format::convert_graphic_type<ElementType_>().template to_vec<3>();

            R<ElementType_> r;
            G<ElementType_> g;
            B<ElementType_> b;

            constexpr RGB() = default;
            constexpr RGB(const ElementType_& r, const ElementType_& g, const ElementType_& b) : r(r), g(g), b(b) {}

            constexpr RGB<ElementType_> operator*(const ElementType_& m) const {
                return {
                    static_cast<ElementType_>(this->r.value * m),
                    static_cast<ElementType_>(this->g.value * m),
                    static_cast<ElementType_>(this->b.value * m)};
            }
            constexpr RGB<ElementType_> operator/(const ElementType_& m) const {
                return {
                    static_cast<ElementType_>(this->r.value / m),
                    static_cast<ElementType_>(this->g.value / m),
                    static_cast<ElementType_>(this->b.value / m)};
            }
            constexpr RGB<ElementType_>& operator*=(const ElementType_& m) {
                this->r.value *= m;
                this->g.value *= m;
                this->b.value *= m;
                return *this;
            }
            constexpr RGB<ElementType_>& operator/=(const ElementType_& m) {
                this->r.value /= m;
                this->g.value /= m;
                this->b.value /= m;
                return *this;
            }

            constexpr RGB<ElementType_> operator+(const RGB<ElementType_>& o) const {
                return {
                    static_cast<ElementType_>(this->r.value + o.r.value),
                    static_cast<ElementType_>(this->g.value + o.g.value),
                    static_cast<ElementType_>(this->b.value + o.b.value)};
            }
            constexpr RGBA<ElementType_> operator+(const RGBA<ElementType_>& o) const;
            constexpr RGB<ElementType_> operator+(const R<ElementType_>& r) const {
                return {static_cast<ElementType_>(this->r.value + r.value), this->g.value, this->b.value};
            }
            constexpr RGB<ElementType_> operator+(const G<ElementType_>& g) const {
                return {this->r.value, static_cast<ElementType_>(this->g.value + g.value), this->b.value};
            }
            constexpr RGB<ElementType_> operator+(const B<ElementType_>& b) const {
                return {this->r.value, this->g.value, static_cast<ElementType_>(this->b.value + b.value)};
            }
            constexpr RGBA<ElementType_> operator+(const A<ElementType_>& a) const {
                return {this->r.value, this->g.value, this->b.value, a.value};
            }

            constexpr RGB<ElementType_> operator-(const RGB<ElementType_>& o) const {
                return {
                    static_cast<ElementType_>(this->r.value - o.r.value),
                    static_cast<ElementType_>(this->g.value - o.g.value),
                    static_cast<ElementType_>(this->b.value - o.b.value)};
            }
            constexpr RGB<ElementType_> operator-(const R<ElementType_>& r) const {
                return {static_cast<ElementType_>(this->r.value - r.value), this->g.value, this->b.value};
            }
            constexpr RGB<ElementType_> operator-(const G<ElementType_>& g) const {
                return {this->r.value, static_cast<ElementType_>(this->g.value - g.value), this->b.value};
            }
            constexpr RGB<ElementType_> operator-(const B<ElementType_>& b) const {
                return {this->r.value, this->g.value, static_cast<ElementType_>(this->b.value - b.value)};
            }
            constexpr RGBA<ElementType_> operator-(const A<ElementType_>& a) const {
                return {this->r.value, this->g.value, this->b.value, static_cast<ElementType_>(-a.value)};
            }

            constexpr RGB<ElementType_>& operator+=(const RGB<ElementType_>& o) {
                r += o.r;
                g += o.g;
                b += o.b;
                return *this;
            }
            constexpr RGB<ElementType_>& operator+=(const R<ElementType_>& r) {
                this->r += r;
                return *this;
            }
            constexpr RGB<ElementType_>& operator+=(const G<ElementType_>& g) {
                this->g += g;
                return *this;
            }
            constexpr RGB<ElementType_>& operator+=(const B<ElementType_>& b) {
                this->b += b;
                return *this;
            }
            constexpr RGB<ElementType_>& operator-=(const RGB<ElementType_>& o) {
                r -= o.r;
                g -= o.g;
                b -= o.b;
                return *this;
            }
            constexpr RGB<ElementType_>& operator-=(const R<ElementType_>& r) {
                this->r -= r;
                return *this;
            }
            constexpr RGB<ElementType_>& operator-=(const G<ElementType_>& g) {
                this->g -= g;
                return *this;
            }
            constexpr RGB<ElementType_>& operator-=(const B<ElementType_>& b) {
                this->b -= b;
                return *this;
            }

            template<typename H = float, typename SV = H>
            constexpr HSV<H, SV> hsv();
        };

        template<typename ElementType_>
        struct RGBA : public RGB<ElementType_> {
            using ElementType = ElementType_;
            static constexpr VSL_NAMESPACE::data_format::Format GRAPHIC_TYPE = VSL_NAMESPACE::data_format::convert_graphic_type<ElementType_>().template to_vec<4>();

            A<ElementType_> a;

            constexpr RGBA() = default;
            constexpr RGBA(const ElementType_& r, const ElementType_& g, const ElementType_& b, const ElementType_& a)
                : RGB<ElementType_>(r, g, b), a(a) {}

            /*
			* alpha値に影響を与えません
			*/
            constexpr RGBA<ElementType_> operator*(const ElementType_& m) const;
            /*
			* alpha値に影響を与えません
			*/
            constexpr RGBA<ElementType_> operator/(const ElementType_& m) const;
            /*
			* alpha値に影響を与えません
			*/
            constexpr RGBA<ElementType_>& operator*=(const ElementType_& m) {
                this->r.value *= m;
                this->g.value *= m;
                this->b.value *= m;
                return *this;
            }
            /*
			* alpha値に影響を与えません
			*/
            constexpr RGBA<ElementType_>& operator/=(const ElementType_& m) {
                this->r.value /= m;
                this->g.value /= m;
                this->b.value /= m;
                return *this;
            }

            // =================================================================

            constexpr RGBA<ElementType_> operator+(const RGB<ElementType_>& o) const;
            constexpr RGBA<ElementType_> operator+(const RGBA<ElementType_>& o) const;
            constexpr RGBA<ElementType_> operator+(const R<ElementType_>& r) const {
                return {static_cast<ElementType_>(this->r.value + r.value), this->g.value, this->b.value, this->a.value};
            }
            constexpr RGBA<ElementType_> operator+(const G<ElementType_>& g) const {
                return {this->r.value, static_cast<ElementType_>(this->g.value + g.value), this->b.value, this->a.value};
            }
            constexpr RGBA<ElementType_> operator+(const B<ElementType_>& b) const {
                return {this->r.value, this->g.value, static_cast<ElementType_>(this->b.value + b.value), this->a.value};
            }
            constexpr RGBA<ElementType_> operator+(const A<ElementType_>& a) const {
                return {this->r.value, this->g.value, this->b.value, static_cast<ElementType_>(this->a.value + a.value)};
            }

            constexpr RGBA<ElementType_> operator-(const RGB<ElementType_>& o) const;
            constexpr RGBA<ElementType_> operator-(const RGBA<ElementType_>& o) const;
            constexpr RGBA<ElementType_> operator-(const R<ElementType_>& r) const {
                return {static_cast<ElementType_>(this->r.value - r.value), this->g.value, this->b.value, this->a.value};
            }
            constexpr RGBA<ElementType_> operator-(const G<ElementType_>& g) const {
                return {this->r.value, static_cast<ElementType_>(this->g.value - g.value), this->b.value, this->a.value};
            }
            constexpr RGBA<ElementType_> operator-(const B<ElementType_>& b) const {
                return {this->r.value, this->g.value, static_cast<ElementType_>(this->b.value - b.value), this->a.value};
            }
            constexpr RGBA<ElementType_> operator-(const A<ElementType_>& a) const {
                return {this->r.value, this->g.value, this->b.value, static_cast<ElementType_>(this->a.value - a.value)};
            }

            constexpr RGBA<ElementType_>& operator+=(const RGB<ElementType_>& o) {
                this->r += o.r;
                this->g += o.g;
                this->b += o.b;
                return *this;
            }
            constexpr RGBA<ElementType_>& operator+=(const RGBA<ElementType_>& o);
            constexpr RGBA<ElementType_>& operator+=(const R<ElementType_>& r) {
                this->r += r;
                return *this;
            }
            constexpr RGBA<ElementType_>& operator+=(const G<ElementType_>& g) {
                this->g += g;
                return *this;
            }
            constexpr RGBA<ElementType_>& operator+=(const B<ElementType_>& b) {
                this->b += b;
                return *this;
            }
            constexpr RGBA<ElementType_>& operator+=(const A<ElementType_>& a) {
                this->a += a;
                return *this;
            }
            constexpr RGBA<ElementType_>& operator-=(const RGB<ElementType_>& o) {
                this->r -= o.r;
                this->g -= o.g;
                this->b -= o.b;
                return *this;
            }
            constexpr RGBA<ElementType_>& operator-=(const RGBA<ElementType_>& o);
            constexpr RGBA<ElementType_>& operator-=(const R<ElementType_>& r) {
                this->r -= r;
                return *this;
            }
            constexpr RGBA<ElementType_>& operator-=(const G<ElementType_>& g) {
                this->g -= g;
                return *this;
            }
            constexpr RGBA<ElementType_>& operator-=(const B<ElementType_>& b) {
                this->b -= b;
                return *this;
            }
            constexpr RGBA<ElementType_>& operator-=(const A<ElementType_>& a) {
                this->a -= a;
                return *this;
            }

            template<typename H = float, typename SV = H>
            constexpr HSVA<H, SV> hsva();
        };

        template<typename HueType_, typename SaturationAndValueType>
        struct HSV {
            using ElementType = HueType_;
            using HueType = HueType_;
            using SaturationType = SaturationAndValueType;
            using ValueType = SaturationAndValueType;
            static constexpr VSL_NAMESPACE::data_format::Format GRAPHIC_TYPE = VSL_NAMESPACE::data_format::convert_graphic_type<HueType_>().template to_vec<3>();

            H<HueType_> h;
            S<SaturationType> s;
            V<ValueType> v;

            constexpr HSV() = default;
            constexpr HSV(const HueType_& h, const SaturationAndValueType& s, const SaturationAndValueType& v) : h(h), s(s), v(v) {}

            constexpr HSV<HueType_, SaturationAndValueType> operator+(const HSV<HueType_, SaturationAndValueType>& o) const {
                return {
                    static_cast<HueType_>(this->h.value + o.h.value),
                    static_cast<SaturationAndValueType>(this->s.value + o.s.value),
                    static_cast<SaturationAndValueType>(this->v.value + o.v.value)};
            }
            constexpr HSVA<HueType_, SaturationAndValueType> operator+(const HSVA<HueType_, SaturationAndValueType>& o) const;
            constexpr HSV<HueType_, SaturationAndValueType> operator+(const H<HueType_>& h) const {
                return {static_cast<HueType_>(this->h.value + h.value), this->s.value, this->v.value};
            }
            constexpr HSV<HueType_, SaturationAndValueType> operator+(const S<SaturationType>& s) const {
                return {this->h.value, static_cast<SaturationAndValueType>(this->s.value + s.value), this->v.value};
            }
            constexpr HSV<HueType_, SaturationAndValueType> operator+(const V<ValueType>& v) const {
                return {this->h.value, this->s.value, static_cast<SaturationAndValueType>(this->v.value + v.value)};
            }
            constexpr HSVA<HueType_, SaturationAndValueType> operator+(const A<SaturationAndValueType>& a) const {
                return {this->h.value, this->s.value, this->v.value, a.value};
            }

            constexpr HSV<HueType_, SaturationAndValueType> operator-(const HSV<HueType_, SaturationAndValueType>& o) const {
                return {
                    static_cast<HueType_>(this->h.value - o.h.value),
                    static_cast<SaturationAndValueType>(this->s.value - o.s.value),
                    static_cast<SaturationAndValueType>(this->v.value - o.v.value)};
            }
            constexpr HSV<HueType_, SaturationAndValueType> operator-(const H<HueType_>& h) const {
                return {static_cast<HueType_>(this->h.value - h.value), this->s.value, this->v.value};
            }
            constexpr HSV<HueType_, SaturationAndValueType> operator-(const S<SaturationType>& s) const {
                return {this->h.value, static_cast<SaturationAndValueType>(this->s.value - s.value), this->v.value};
            }
            constexpr HSV<HueType_, SaturationAndValueType> operator-(const V<ValueType>& v) const {
                return {this->h.value, this->s.value, static_cast<SaturationAndValueType>(this->v.value - v.value)};
            }
            constexpr HSVA<HueType_, SaturationAndValueType> operator-(const A<SaturationAndValueType>& a) const {
                return {this->h.value, this->s.value, this->v.value, static_cast<SaturationAndValueType>(-a.value)};
            }

            constexpr HSV<HueType_, SaturationAndValueType>& operator+=(const HSV<HueType_, SaturationAndValueType>& o) {
                this->h += o.h;
                this->s += o.s;
                this->v += o.v;
                return *this;
            }
            constexpr HSV<HueType_, SaturationAndValueType>& operator+=(const H<HueType_>& h) {
                this->h += h;
                return *this;
            }
            constexpr HSV<HueType_, SaturationAndValueType>& operator+=(const S<SaturationType>& s) {
                this->s += s;
                return *this;
            }
            constexpr HSV<HueType_, SaturationAndValueType>& operator+=(const V<ValueType>& v) {
                this->v += v;
                return *this;
            }
            constexpr HSV<HueType_, SaturationAndValueType>& operator-=(const HSV<HueType_, SaturationAndValueType>& o) {
                this->h -= o.h;
                this->s -= o.s;
                this->v -= o.v;
                return *this;
            }
            constexpr HSV<HueType_, SaturationAndValueType>& operator-=(const H<HueType_>& h) {
                this->h -= h;
                return *this;
            }
            constexpr HSV<HueType_, SaturationAndValueType>& operator-=(const S<SaturationType>& s) {
                this->s -= s;
                return *this;
            }
            constexpr HSV<HueType_, SaturationAndValueType>& operator-=(const V<ValueType>& v) {
                this->v -= v;
                return *this;
            }

            template<typename T = float>
            constexpr RGB<T> rgb();
        };

        template<typename HueType_, typename SaturationAndValueType>
        struct HSVA : public HSV<HueType_, SaturationAndValueType> {
            using ElementType = HueType_;
            using HueType = HueType_;
            using SaturationType = SaturationAndValueType;
            using ValueType = SaturationAndValueType;
            using AlphaType = SaturationAndValueType;
            static constexpr VSL_NAMESPACE::data_format::Format GRAPHIC_TYPE = VSL_NAMESPACE::data_format::convert_graphic_type<HueType_>().template to_vec<4>();

            A<AlphaType> a;

            constexpr HSVA() = default;
            constexpr HSVA(const HueType_& h, const SaturationAndValueType& s, const SaturationAndValueType& v, const SaturationAndValueType& a)
                : HSV<HueType_, SaturationAndValueType>(h, s, v), a(a) {}

            constexpr HSVA<HueType_, SaturationAndValueType> operator+(const HSV<HueType_, SaturationAndValueType>& o) const;
            constexpr HSVA<HueType_, SaturationAndValueType> operator+(const HSVA<HueType_, SaturationAndValueType>& o) const;
            constexpr HSVA<HueType_, SaturationAndValueType> operator+(const H<HueType_>& h) const {
                return {static_cast<HueType_>(this->h.value + h.value), this->s.value, this->v.value, a.value};
            }
            constexpr HSVA<HueType_, SaturationAndValueType> operator+(const S<SaturationType>& s) const {
                return {this->h.value, static_cast<SaturationAndValueType>(this->s.value + s.value), this->v.value, a.value};
            }
            constexpr HSVA<HueType_, SaturationAndValueType> operator+(const V<ValueType>& v) const {
                return {this->h.value, this->s.value, static_cast<SaturationAndValueType>(this->v.value + v.value), a.value};
            }
            constexpr HSVA<HueType_, SaturationAndValueType> operator+(const A<AlphaType>& a) const {
                return {this->h.value, this->s.value, this->v.value, static_cast<SaturationAndValueType>(this->a.value + a.value)};
            }

            constexpr HSVA<HueType_, SaturationAndValueType> operator-(const HSV<HueType_, SaturationAndValueType>& o) const;
            constexpr HSVA<HueType_, SaturationAndValueType> operator-(const HSVA<HueType_, SaturationAndValueType>& o) const;
            constexpr HSVA<HueType_, SaturationAndValueType> operator-(const H<HueType_>& h) const {
                return {static_cast<HueType_>(this->h.value - h.value), this->s.value, this->v.value, a.value};
            }
            constexpr HSVA<HueType_, SaturationAndValueType> operator-(const S<SaturationType>& s) const {
                return {this->h.value, static_cast<SaturationAndValueType>(this->s.value - s.value), this->v.value, a.value};
            }
            constexpr HSVA<HueType_, SaturationAndValueType> operator-(const V<ValueType>& v) const {
                return {this->h.value, this->s.value, static_cast<SaturationAndValueType>(this->v.value - v.value), a.value};
            }
            constexpr HSVA<HueType_, SaturationAndValueType> operator-(const A<AlphaType>& a) const {
                return {this->h.value, this->s.value, this->v.value, static_cast<SaturationAndValueType>(this->a.value - a.value)};
            }

            constexpr HSVA<HueType_, SaturationAndValueType>& operator+=(const HSV<HueType_, SaturationAndValueType>& o) {
                this->h += o.h;
                this->s += o.s;
                this->v += o.v;
                return *this;
            }
            constexpr HSVA<HueType_, SaturationAndValueType>& operator+=(const HSVA<HueType_, SaturationAndValueType>& o);
            constexpr HSVA<HueType_, SaturationAndValueType>& operator+=(const H<HueType_>& h) {
                this->h += h;
                return *this;
            }
            constexpr HSVA<HueType_, SaturationAndValueType>& operator+=(const S<SaturationType>& s) {
                this->s += s;
                return *this;
            }
            constexpr HSVA<HueType_, SaturationAndValueType>& operator+=(const V<ValueType>& v) {
                this->v += v;
                return *this;
            }
            constexpr HSVA<HueType_, SaturationAndValueType>& operator+=(const A<AlphaType>& a) {
                this->a += a;
                return *this;
            }
            constexpr HSVA<HueType_, SaturationAndValueType>& operator-=(const HSV<HueType_, SaturationAndValueType>& o) {
                this->h -= o.h;
                this->s -= o.s;
                this->v -= o.v;
                return *this;
            }
            constexpr HSVA<HueType_, SaturationAndValueType>& operator-=(const HSVA<HueType_, SaturationAndValueType>& o);
            constexpr HSVA<HueType_, SaturationAndValueType>& operator-=(const H<HueType_>& h) {
                this->h -= h;
                return *this;
            }
            constexpr HSVA<HueType_, SaturationAndValueType>& operator-=(const S<SaturationType>& s) {
                this->s -= s;
                return *this;
            }
            constexpr HSVA<HueType_, SaturationAndValueType>& operator-=(const V<ValueType>& v) {
                this->v -= v;
                return *this;
            }
            constexpr HSVA<HueType_, SaturationAndValueType>& operator-=(const A<AlphaType>& a) {
                this->a -= a;
                return *this;
            }

            template<typename T = float>
            constexpr RGBA<T> rgba();
        };

        using IntRGB = color::RGB<int8_t>;
        using IntRGBA = color::RGBA<int8_t>;
        using FloatRGB = color::RGB<float>;
        using FloatRGBA = color::RGBA<float>;
        using IntHSV = color::HSV<short, int8_t>;
        using IntHSVA = color::HSVA<short, int8_t>;
        using FloatHSV = color::HSV<float, float>;
        using FloatHSVA = color::HSVA<float, float>;

        template<typename T>
        std::ostream& operator<<(std::ostream& out, const R<T>& r) {
            out << "red : " << r.value;
            return out;
        };

        template<typename T>
        std::ostream& operator<<(std::ostream& out, const G<T>& g) {
            out << "green : " << g.value;
            return out;
        };

        template<typename T>
        std::ostream& operator<<(std::ostream& out, const B<T>& b) {
            out << "blue : " << b.value;
            return out;
        };

        template<typename T>
        std::ostream& operator<<(std::ostream& out, const A<T>& a) {
            out << "alpha : " << a.value;
            return out;
        };

        template<typename T>
        std::ostream& operator<<(std::ostream& out, const H<T>& h) {
            out << "hue : " << h.value;
            return out;
        };

        template<typename T>
        std::ostream& operator<<(std::ostream& out, const S<T>& s) {
            out << "saturation : " << s.value;
            return out;
        };

        template<typename T>
        std::ostream& operator<<(std::ostream& out, const V<T>& v) {
            out << "brightness : " << v.value;
            return out;
        };

        template<typename ElementType_>
        constexpr RGBA<ElementType_> RGB<ElementType_>::operator+(const RGBA<ElementType_>& o) const {
            return {
                static_cast<ElementType_>(this->r.value + o.r.value),
                static_cast<ElementType_>(this->g.value + o.g.value),
                static_cast<ElementType_>(this->b.value + o.b.value),
                o.a.value};
        }

        template<typename ElementType_>
        template<typename H, typename SV>
        constexpr HSV<H, SV> RGB<ElementType_>::hsv() {
            using FloatType = std::conditional_t<
                std::is_floating_point_v<ElementType_>, ElementType_, double>; // 浮動小数点型を確保
            using ChannelType = ElementType_;

            FloatType r = static_cast<FloatType>(this->r.value);
            FloatType g = static_cast<FloatType>(this->g.value);
            FloatType b = static_cast<FloatType>(this->b.value);

            if constexpr (std::is_integral_v<ChannelType>) {
                r /= std::numeric_limits<ChannelType>::max();
                g /= std::numeric_limits<ChannelType>::max();
                b /= std::numeric_limits<ChannelType>::max();
            }

            FloatType max_val = std::max({r, g, b});
            FloatType min_val = std::min({r, g, b});
            FloatType delta = max_val - min_val;

            FloatType h = 0;
            FloatType s = (max_val == 0) ? 0 : (delta / max_val);
            FloatType v = max_val;

            if (delta != 0) {
                if (max_val == r) {
                    h = 60 * (std::fmod(((g - b) / delta), 6));
                } else if (max_val == g) {
                    h = 60 * (((b - r) / delta) + 2);
                } else {
                    h = 60 * (((r - g) / delta) + 4);
                }
            }
            if (h < 0)
                h += 360;

            if constexpr (std::is_integral_v<H>) {
                h = std::round(h);
            }
            if constexpr (std::is_integral_v<SV>) {
                s = std::round(s * std::numeric_limits<SV>::max());
                v = std::round(v * std::numeric_limits<SV>::max());
            }

            return {static_cast<H>(h), static_cast<SV>(s), static_cast<SV>(v)};
        }

        template<typename ElementType_>
        constexpr RGBA<ElementType_> RGBA<ElementType_>::operator*(const ElementType_& m) const {
            return {
                static_cast<ElementType_>(this->r.value * m),
                static_cast<ElementType_>(this->g.value * m),
                static_cast<ElementType_>(this->b.value * m),
                this->a.value};
        }

        template<typename ElementType_>
        constexpr RGBA<ElementType_> RGBA<ElementType_>::operator/(const ElementType_& m) const {
            return {
                static_cast<ElementType_>(this->r.value / m),
                static_cast<ElementType_>(this->g.value / m),
                static_cast<ElementType_>(this->b.value / m),
                this->a.value};
        }

        template<typename ElementType_>
        constexpr RGBA<ElementType_> RGBA<ElementType_>::operator+(const RGB<ElementType_>& o) const {
            return {
                static_cast<ElementType_>(this->r.value + o.r.value),
                static_cast<ElementType_>(this->g.value + o.g.value),
                static_cast<ElementType_>(this->b.value + o.b.value),
                this->a.value};
        }

        template<typename ElementType_>
        constexpr RGBA<ElementType_> RGBA<ElementType_>::operator+(const RGBA<ElementType_>& o) const {
            return {
                static_cast<ElementType_>(this->r.value + o.r.value),
                static_cast<ElementType_>(this->g.value + o.g.value),
                static_cast<ElementType_>(this->b.value + o.b.value),
                static_cast<ElementType_>(this->a.value + o.a.value)};
        }

        template<typename ElementType_>
        constexpr RGBA<ElementType_> RGBA<ElementType_>::operator-(const RGB<ElementType_>& o) const {
            return {
                static_cast<ElementType_>(this->r.value - o.r.value),
                static_cast<ElementType_>(this->g.value - o.g.value),
                static_cast<ElementType_>(this->b.value - o.b.value),
                this->a.value};
        }

        template<typename ElementType_>
        constexpr RGBA<ElementType_> RGBA<ElementType_>::operator-(const RGBA<ElementType_>& o) const {
            return {
                static_cast<ElementType_>(this->r.value - o.r.value),
                static_cast<ElementType_>(this->g.value - o.g.value),
                static_cast<ElementType_>(this->b.value - o.b.value),
                static_cast<ElementType_>(this->a.value - o.a.value)};
        }

        template<typename ElementType_>
        constexpr RGBA<ElementType_>& RGBA<ElementType_>::operator+=(const RGBA<ElementType_>& o) {
            this->r += o.r;
            this->g += o.g;
            this->b += o.b;
            this->a += o.a;
            return *this;
        }

        template<typename ElementType_>
        constexpr RGBA<ElementType_>& RGBA<ElementType_>::operator-=(const RGBA<ElementType_>& o) {
            this->r -= o.r;
            this->g -= o.g;
            this->b -= o.b;
            this->a -= o.a;
            return *this;
        }

        template<typename ElementType_>
        template<typename H, typename SV>
        constexpr HSVA<H, SV> RGBA<ElementType_>::hsva() {
            using FloatType = std::conditional_t<
                std::is_floating_point_v<ElementType_>, ElementType_, double>; // 浮動小数点型を確保
            using ChannelType = ElementType_;

            FloatType a = static_cast<FloatType>(this->a.value);

            if constexpr (std::is_integral_v<ElementType_>)
                a /= std::numeric_limits<ElementType_>::max();

            if constexpr (std::is_integral_v<SV>)
                a = std::round(a * std::numeric_limits<SV>::max());

            const auto hsv = this->template hsv<H, SV>();
            return {hsv.h.value, hsv.s.value, hsv.v.value, static_cast<SV>(a)};
        }

        template<typename HueType_, typename SaturationAndValueType>
        constexpr HSVA<HueType_, SaturationAndValueType> HSV<HueType_, SaturationAndValueType>::operator+(const HSVA<HueType_, SaturationAndValueType>& o) const {
            return {
                static_cast<HueType_>(this->h.value + o.h.value),
                static_cast<SaturationAndValueType>(this->s.value + o.s.value),
                static_cast<SaturationAndValueType>(this->v.value + o.v.value),
                o.a.value};
        }

        template<typename HueType_, typename SaturationAndValueType>
        template<typename T>
        constexpr RGB<T> HSV<HueType_, SaturationAndValueType>::rgb() {
            using FloatType = std::conditional_t<
                std::is_floating_point_v<SaturationAndValueType>, SaturationAndValueType, double>; // 浮動小数点型を確保
            using ChannelType = T;

            FloatType h = static_cast<FloatType>(this->h.value);
            FloatType s = static_cast<FloatType>(this->s.value);
            FloatType v = static_cast<FloatType>(this->v.value);

            if constexpr (std::is_integral_v<SaturationAndValueType>) {
                s /= std::numeric_limits<SaturationAndValueType>::max();
                v /= std::numeric_limits<SaturationAndValueType>::max();
            }

            FloatType c = v * s;
            FloatType x = c * (1 - std::fabs(std::fmod(h / 60.0, 2) - 1));
            FloatType m = v - c;

            FloatType r, g, b;
            if (h < 60) {
                r = c;
                g = x;
                b = 0;
            } else if (h < 120) {
                r = x;
                g = c;
                b = 0;
            } else if (h < 180) {
                r = 0;
                g = c;
                b = x;
            } else if (h < 240) {
                r = 0;
                g = x;
                b = c;
            } else if (h < 300) {
                r = x;
                g = 0;
                b = c;
            } else {
                r = c;
                g = 0;
                b = x;
            }

            r += m;
            g += m;
            b += m;

            if constexpr (std::is_integral_v<T>) {
                constexpr T max_val = std::numeric_limits<T>::max();
                return {static_cast<T>(std::round(r * max_val)),
                        static_cast<T>(std::round(g * max_val)),
                        static_cast<T>(std::round(b * max_val))};
            } else {
                return {static_cast<T>(r), static_cast<T>(g), static_cast<T>(b)};
            }
        }

        template<typename HueType_, typename SaturationAndValueType>
        constexpr HSVA<HueType_, SaturationAndValueType> HSVA<HueType_, SaturationAndValueType>::operator+(const HSV<HueType_, SaturationAndValueType>& o) const {
            return {
                static_cast<HueType_>(this->h.value + o.h.value),
                static_cast<SaturationAndValueType>(this->s.value + o.s.value),
                static_cast<SaturationAndValueType>(this->v.value + o.v.value),
                a.value};
        }

        template<typename HueType_, typename SaturationAndValueType>
        constexpr HSVA<HueType_, SaturationAndValueType> HSVA<HueType_, SaturationAndValueType>::operator+(const HSVA<HueType_, SaturationAndValueType>& o) const {
            return {
                static_cast<HueType_>(this->h.value + o.h.value),
                static_cast<SaturationAndValueType>(this->s.value + o.s.value),
                static_cast<SaturationAndValueType>(this->v.value + o.v.value),
                static_cast<SaturationAndValueType>(a.value + o.a.value)};
        }

        template<typename HueType_, typename SaturationAndValueType>
        constexpr HSVA<HueType_, SaturationAndValueType> HSVA<HueType_, SaturationAndValueType>::operator-(const HSV<HueType_, SaturationAndValueType>& o) const {
            return {
                static_cast<HueType_>(this->h.value - o.h.value),
                static_cast<SaturationAndValueType>(this->s.value - o.s.value),
                static_cast<SaturationAndValueType>(this->v.value - o.v.value),
                a.value};
        }

        template<typename HueType_, typename SaturationAndValueType>
        constexpr HSVA<HueType_, SaturationAndValueType> HSVA<HueType_, SaturationAndValueType>::operator-(const HSVA<HueType_, SaturationAndValueType>& o) const {
            return {
                static_cast<HueType_>(this->h.value - o.h.value),
                static_cast<SaturationAndValueType>(this->s.value - o.s.value),
                static_cast<SaturationAndValueType>(this->v.value - o.v.value),
                static_cast<SaturationAndValueType>(a.value - o.a.value)};
        }

        template<typename HueType_, typename SaturationAndValueType>
        constexpr HSVA<HueType_, SaturationAndValueType>& HSVA<HueType_, SaturationAndValueType>::operator+=(const HSVA<HueType_, SaturationAndValueType>& o) {
            this->h += o.h;
            this->s += o.s;
            this->v += o.v;
            this->a += o.a;
            return *this;
        }

        template<typename HueType_, typename SaturationAndValueType>
        constexpr HSVA<HueType_, SaturationAndValueType>& HSVA<HueType_, SaturationAndValueType>::operator-=(const HSVA<HueType_, SaturationAndValueType>& o) {
            this->h -= o.h;
            this->s -= o.s;
            this->v -= o.v;
            this->a -= o.a;
            return *this;
        }

        template<typename HueType_, typename SaturationAndValueType>
        template<typename T>
        constexpr RGBA<T> HSVA<HueType_, SaturationAndValueType>::rgba() {
            using FloatType = std::conditional_t<
                std::is_floating_point_v<SaturationAndValueType>, SaturationAndValueType, double>; // 浮動小数点型を確保
            using ChannelType = T;

            FloatType a = static_cast<FloatType>(this->a.value);

            if constexpr (std::is_integral_v<SaturationAndValueType>) {
                a /= std::numeric_limits<SaturationAndValueType>::max();
            }

            const auto rgb = this->template rgb<T>();
            if constexpr (std::is_integral_v<T>)
                a = std::round(a * std::numeric_limits<ChannelType>::max());
            return {rgb.r.value, rgb.g.value, rgb.b.value, static_cast<ChannelType>(a)};
        }

        template<typename T>
        std::ostream& operator<<(std::ostream& out, const RGB<T>& rgb) {
            out << "(r, g, b)=" << "(" << rgb.r.value << ", " << rgb.g.value
                << ", " << rgb.b.value << ")";
            return out;
        };

        template<typename T>
        std::ostream& operator<<(std::ostream& out, const RGBA<T>& rgb) {
            out << "(r, g, b, a)=" << "(" << rgb.r.value << ", " << rgb.g.value
                << ", " << rgb.b.value << ", " << rgb.a.value << ")";
            return out;
        };

        template<typename T>
        std::ostream& operator<<(std::ostream& out, const HSV<T>& rgb) {
            out << "(h, s, v)=" << "(" << rgb.h.value << ", " << rgb.s.value
                << ", " << rgb.v.value << ")";
            return out;
        };

        template<typename T>
        std::ostream& operator<<(std::ostream& out, const HSVA<T>& rgb) {
            out << "(h, s, v, a)=" << "(" << rgb.h.value << ", " << rgb.s.value
                << ", " << rgb.v.value << ", " << rgb.a.value << ")";
            return out;
        };

        template<concepts::IS_RGB_ELEMENET L, concepts::IS_RGB_ELEMENET R>
        requires(!std::convertible_to<L, R>) && std::same_as<typename L::ElementType, typename R::ElementType>
        RGB<typename L::ElementType> operator+(const L& l, const R& r) {
            RGB<typename L::ElementType> rgb;
            rgb += l;
            rgb += r;
            return rgb;
        }

        template<concepts::IS_HSV_ELEMENET L, concepts::IS_HSV_ELEMENET R>
        requires(!std::convertible_to<L, R>) && std::same_as<typename L::ElementType, typename R::ElementType>
        HSV<typename L::ElementType> operator+(const L& l, const R& r) {
            HSV<typename L::ElementType> hsv;
            hsv += l;
            hsv += r;
            return hsv;
        }

        template<concepts::IS_RGB_ELEMENET L, typename ElementType_>
        requires std::same_as<typename L::ElementType, ElementType_>
        RGBA<typename L::ElementType> operator+(const L& l, A<ElementType_> a) {
            RGBA<typename L::ElementType> rgb;
            rgb += l;
            rgb += a;
            return rgb;
        }

        template<concepts::IS_HSV_ELEMENET L, typename ElementType_>
        requires std::same_as<typename L::ElementType, ElementType_>
        HSVA<typename L::ElementType> operator+(const L& l, A<ElementType_> a) {
            HSVA<typename L::ElementType> hsv;
            hsv += l;
            hsv += a;
            return hsv;
        }

        template<typename ElementType_, concepts::IS_RGB_ELEMENET R>
        requires std::same_as<typename R::ElementType, ElementType_>
        RGBA<typename R::ElementType> operator+(A<ElementType_> a, const R& r) {
            RGBA<typename R::ElementType> rgb;
            rgb += r;
            rgb += a;
            return rgb;
        }

        template<typename ElementType_, concepts::IS_HSV_ELEMENET R>
        requires std::same_as<typename R::ElementType, ElementType_>
        HSVA<typename R::ElementType> operator+(A<ElementType_> a, const R& r) {
            HSVA<typename R::ElementType> rgb;
            rgb += r;
            rgb += a;
            return rgb;
        }

        template<concepts::IS_RGB_ELEMENET L, concepts::IS_RGB_ELEMENET R>
        requires(!std::convertible_to<L, R>) && std::same_as<typename L::ElementType, typename R::ElementType>
        RGB<typename L::ElementType> operator-(const L& l, const R& r) {
            RGB<typename L::ElementType> rgb;
            rgb += l;
            rgb -= r;
            return rgb;
        }

        template<concepts::IS_HSV_ELEMENET L, concepts::IS_HSV_ELEMENET R>
        requires(!std::convertible_to<L, R>) && std::same_as<typename L::ElementType, typename R::ElementType>
        HSV<typename L::ElementType> operator-(const L& l, const R& r) {
            HSV<typename L::ElementType> hsv;
            hsv += l;
            hsv -= r;
            return hsv;
        }

        template<concepts::IS_RGB_ELEMENET L, typename ElementType_>
        requires std::same_as<typename L::ElementType, ElementType_>
        RGBA<typename L::ElementType> operator-(const L& l, const A<ElementType_>& a) {
            RGBA<typename L::ElementType> rgb;
            rgb += l;
            rgb -= a;
            return rgb;
        }

        template<concepts::IS_HSV_ELEMENET L, typename ElementType_>
        requires std::same_as<typename L::ElementType, ElementType_>
        HSVA<typename L::ElementType> operator-(const L& l, const A<ElementType_>& a) {
            HSVA<typename L::ElementType> hsv;
            hsv += l;
            hsv -= a;
            return hsv;
        }

        template<typename ElementType_, concepts::IS_RGB_ELEMENET R>
        requires std::same_as<typename R::ElementType, ElementType_>
        RGBA<typename R::ElementType> operator-(const A<ElementType_>& a, const R& r) {
            RGBA<typename R::ElementType> rgb;
            rgb -= r;
            rgb += a;
            return rgb;
        }

        template<typename ElementType_, concepts::IS_HSV_ELEMENET R>
        requires std::same_as<typename R::ElementType, ElementType_>
        HSVA<typename R::ElementType> operator-(const A<ElementType_>& a, const R& r) {
            HSVA<typename R::ElementType> rgb;
            rgb -= r;
            rgb += a;
            return rgb;
        }

        namespace literals {
#define LITERAL_EXPANDER(Upper, Lower)                                              \
    constexpr Upper<double> operator AHO_LITERAL(Lower)(long double v) {            \
        return {(double)v};                                                         \
    };                                                                              \
    constexpr Upper<int> operator AHO_LITERAL(Lower)(unsigned long long v) {        \
        return {(int)v};                                                            \
    };                                                                              \
    constexpr Upper<float> operator AHO_LITERAL(f_##Lower)(long double v) {         \
        return {(float)v};                                                          \
    };                                                                              \
    constexpr Upper<size_t> operator AHO_LITERAL(l_##Lower)(unsigned long long v) { \
        return {v};                                                                 \
    };                                                                              \
    constexpr Upper<double> Upper##_(1.0);

            LITERAL_EXPANDER(R, r);
            LITERAL_EXPANDER(G, g);
            LITERAL_EXPANDER(B, b);
            LITERAL_EXPANDER(H, h);
            LITERAL_EXPANDER(S, s);
            LITERAL_EXPANDER(V, v);
            LITERAL_EXPANDER(A, a);
        } // namespace literals
    } // namespace color

    using RGB = color::RGB<float>;
    using RGBA = color::RGBA<float>;
    using HSV = color::HSV<float, float>;
    using HSVA = color::HSVA<float, float>;

    namespace concepts {
        using namespace color::concepts;
    }

    namespace literals {
        using namespace color::literals;
    }
} // namespace AHO_NAMESPACE

#undef RGB_ELEMENT_TYPE_DEFINITION_EXPANDER
#undef HSV_ELEMENT_TYPE_DEFINITION_EXPANDER
#undef LITERAL_EXPANDER
