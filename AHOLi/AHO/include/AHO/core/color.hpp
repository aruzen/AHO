#pragma once
#pragma once
#include "../define.hpp"
#include "../concepts.hpp"

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

#define RGB_ELEMENT_TYPE_DEFINITION_EXPANDER(Upper, Lower) \
template <typename T> \
struct Upper { \
	using element_type = T; \
	constexpr static bool is_rgb_elemenet = true; \
 \
	T value; \
 \
	constexpr static char UpperChar = #Upper[0]; \
	constexpr static char LowerChar = #Lower[0]; \
 \
	constexpr Upper(const T& t) : value(t) {}; \
	constexpr Upper() : value{} {}; \
 \
	constexpr Upper<T> operator +() const { static_assert(concepts::plusable<T>); return { value }; }; \
	constexpr Upper<T> operator -() const { static_assert(concepts::minusable<T>); return { -value }; }; \
	template <concepts::add_as<T> RV> \
	constexpr Upper<decltype(std::declval<T>() + std::declval<RV>())> operator +(const Upper<RV>& t) const { return { value + t.value }; }; \
	template <concepts::subtract_as<T> RV> \
	constexpr Upper<decltype(std::declval<T>() - std::declval<RV>())> operator -(const Upper<RV>& t) const { return { value - t.value }; }; \
	template <concepts::multiply_as<T> RV> \
	constexpr Upper<decltype(std::declval<T>()* std::declval<RV>())> operator *(const Upper<RV>& t) const { return { value * t.value }; }; \
	template <concepts::division_as<T> RV> \
	constexpr Upper<decltype(std::declval<T>() / std::declval<RV>())> operator /(const Upper<RV>& t) const { return { value / t.value }; }; \
 \
	template <concepts::add_as<T> RV> \
	constexpr Upper<T>& operator +=(const Upper<RV>& t) { value += t.value; return *this; }; \
	template <concepts::subtract_as<T> RV> \
	constexpr Upper<T>& operator -=(const Upper<RV>& t) { value -= t.value; return *this; }; \
	template <concepts::multiply_as<T> RV> \
	constexpr Upper<T>& operator *=(const Upper<RV>& t) { value *= t.value; return *this; }; \
	template <concepts::division_as<T> RV> \
	constexpr Upper<T>& operator /=(const Upper<RV>& t) { value /= t.value; return *this; }; \
 \
	template <concepts::cast_as<T> RV> \
	Upper<RV> cast() const { return { (RV)value }; } \
};

#define HSV_ELEMENT_TYPE_DEFINITION_EXPANDER(Upper, Lower) \
template <typename T> \
struct Upper { \
	using element_type = T; \
	constexpr static bool is_hsv_elemenet = true; \
 \
	T value; \
 \
	constexpr static char UpperChar = #Upper[0]; \
	constexpr static char LowerChar = #Lower[0]; \
 \
	constexpr Upper(const T& t) : value(t) {}; \
	constexpr Upper() : value{} {}; \
 \
	constexpr Upper<T> operator +() const { static_assert(concepts::plusable<T>); return { value }; }; \
	constexpr Upper<T> operator -() const { static_assert(concepts::minusable<T>); return { -value }; }; \
	template <concepts::add_as<T> RV> \
	constexpr Upper<decltype(std::declval<T>() + std::declval<RV>())> operator +(const Upper<RV>& t) const { return { value + t.value }; }; \
	template <concepts::subtract_as<T> RV> \
	constexpr Upper<decltype(std::declval<T>() - std::declval<RV>())> operator -(const Upper<RV>& t) const { return { value - t.value }; }; \
	template <concepts::multiply_as<T> RV> \
	constexpr Upper<decltype(std::declval<T>()* std::declval<RV>())> operator *(const Upper<RV>& t) const { return { value * t.value }; }; \
	template <concepts::division_as<T> RV> \
	constexpr Upper<decltype(std::declval<T>() / std::declval<RV>())> operator /(const Upper<RV>& t) const { return { value / t.value }; }; \
 \
	template <concepts::add_as<T> RV> \
	constexpr Upper<T>& operator +=(const Upper<RV>& t) { value += t.value; return *this; }; \
	template <concepts::subtract_as<T> RV> \
	constexpr Upper<T>& operator -=(const Upper<RV>& t) { value -= t.value; return *this; }; \
	template <concepts::multiply_as<T> RV> \
	constexpr Upper<T>& operator *=(const Upper<RV>& t) { value *= t.value; return *this; }; \
	template <concepts::division_as<T> RV> \
	constexpr Upper<T>& operator /=(const Upper<RV>& t) { value /= t.value; return *this; }; \
 \
	template <concepts::cast_as<T> RV> \
	Upper<RV> cast() const { return { (RV)value }; } \
};

		template <typename T>
		struct A {
			using element_type = T;

			T value;

			constexpr static char UpperChar = 'A';
			constexpr static char LowerChar = 'a';

			constexpr A(const T& t) : value(t) {};
			constexpr A() : value{} {};

			constexpr A<T> operator +() const { static_assert(concepts::plusable<T>); return { value }; };
			constexpr A<T> operator -() const { static_assert(concepts::minusable<T>); return { -value }; };
			template <concepts::add_as<T> R>
			constexpr A<decltype(std::declval<T>() + std::declval<R>())> operator +(A<R> t) const { return { value + t.value }; };
			template <concepts::subtract_as<T> R>
			constexpr A<decltype(std::declval<T>() - std::declval<R>())> operator -(A<R> t) const { return { value - t.value }; };
			template <concepts::multiply_as<T> R>
			constexpr A<decltype(std::declval<T>()* std::declval<R>())> operator *(A<R> t) const { return { value * t.value }; };
			template <concepts::division_as<T> R>
			constexpr A<decltype(std::declval<T>() / std::declval<R>())> operator /(A<R> t) const { return { value / t.value }; };

			template <concepts::add_as<T> R>
			constexpr A<T>& operator +=(A<R> t) { value += t.value; return *this; };
			template <concepts::subtract_as<T> R>
			constexpr A<T>& operator -=(A<R> t) { value -= t.value; return *this; };
			template <concepts::multiply_as<T> R>
			constexpr A<T>& operator *=(A<R> t) { value *= t.value; return *this; };
			template <concepts::division_as<T> R>
			constexpr A<T>& operator /=(A<R> t) { value /= t.value; return *this; };

			template <concepts::cast_as<T> R>
			A<R> cast() const { return { (R)value }; }
		};

		RGB_ELEMENT_TYPE_DEFINITION_EXPANDER(R, r);
		RGB_ELEMENT_TYPE_DEFINITION_EXPANDER(G, g);
		RGB_ELEMENT_TYPE_DEFINITION_EXPANDER(B, b);

		HSV_ELEMENT_TYPE_DEFINITION_EXPANDER(H, h);
		HSV_ELEMENT_TYPE_DEFINITION_EXPANDER(S, s);
		HSV_ELEMENT_TYPE_DEFINITION_EXPANDER(V, v);

		namespace concepts {
			template<typename T>
			concept is_rgb_elemenet = T::is_rgb_elemenet;

			template<typename T>
			concept is_hsv_elemenet = T::is_hsv_elemenet;
		}

		template<typename T>
		std::ostream& operator <<(std::ostream& out, const R<T>& r) {
			out << "red : " << r.value;
			return out;
		};

		template<typename T>
		std::ostream& operator <<(std::ostream& out, const G<T>& g) {
			out << "green : " << g.value;
			return out;
		};

		template<typename T>
		std::ostream& operator <<(std::ostream& out, const B<T>& b) {
			out << "blue : " << b.value;
			return out;
		};

		template<typename T>
		std::ostream& operator <<(std::ostream& out, const A<T>& a) {
			out << "alpha : " << a.value;
			return out;
		};

		template<typename T>
		std::ostream& operator <<(std::ostream& out, const H<T>& h) {
			out << "hue : " << h.value;
			return out;
		};

		template<typename T>
		std::ostream& operator <<(std::ostream& out, const S<T>& s) {
			out << "saturation : " << s.value;
			return out;
		};

		template<typename T>
		std::ostream& operator <<(std::ostream& out, const V<T>& v) {
			out << "brightness : " << v.value;
			return out;
		};

		template<typename ElementType>
		struct RGB {
			using element_type = ElementType;
			static constexpr VSL_NAMESPACE::data_format::___Format graphic_type
				= VSL_NAMESPACE::data_format::convert_graphic_type<ElementType>().template toVec<3>();

			R<ElementType> r;
			G<ElementType> g;
			B<ElementType> b;

			constexpr RGB() = default;
			constexpr RGB(const ElementType& r, const ElementType& g, const ElementType& b) : r(r), g(g), b(b) {}

			constexpr RGB<ElementType> operator * (const ElementType& m) const {
				return { this->r * m, this->g * m, this->b * m };
			}
			constexpr RGB<ElementType> operator / (const ElementType& m) const {
				return { this->r / m, this->g / m, this->b / m };
			}
			constexpr RGB<ElementType>& operator *= (const ElementType& m) {
				this->r *= m;
				this->g *= m;
				this->b *= m;
				return *this;
			}
			constexpr RGB<ElementType>& operator /= (const ElementType& m) {
				this->r /= m;
				this->g /= m;
				this->b /= m;
				return *this;
			}

			constexpr RGB<ElementType> operator + (const RGB<ElementType>& o) const {
				return { this->r + o.r, this->g + o.g, this->b + o.b };
			}
			constexpr RGBA<ElementType> operator + (const RGBA<ElementType>& o) const {
				return { this->r + o.r, this->g + o.g, this->b + o.b, o.a };
			}
			constexpr RGB<ElementType> operator + (const R<ElementType>& r) const {
				return { this->r + r, this->g, this->b };
			}
			constexpr RGB<ElementType> operator + (const G<ElementType>& g) const {
				return { this->r, this->g + g, this->b };
			}
			constexpr RGB<ElementType> operator + (const B<ElementType>& b) const {
				return { this->r, this->g, this->b + b };
			}
			constexpr RGBA<ElementType> operator + (const A<ElementType>& a) const {
				return { this->r, this->g, this->b, a };
			}

			constexpr RGB<ElementType> operator - (const RGB<ElementType>& o) const {
				return { this->r - o.r, this->g - o.g, this->b - o.b };
			}
			constexpr RGB<ElementType> operator - (const R<ElementType>& r) const {
				return { this->r - r, this->g, this->b };
			}
			constexpr RGB<ElementType> operator - (const G<ElementType>& g) const {
				return { this->r, this->g - g, this->b };
			}
			constexpr RGB<ElementType> operator - (const B<ElementType>& b) const {
				return { this->r, this->g, this->b - b };
			}
			constexpr RGBA<ElementType> operator - (const A<ElementType>& a) const {
				return { this->r, this->g, this->b, a };
			}

			constexpr RGB<ElementType>& operator += (const RGB<ElementType>& o) {
				*this = RGB<ElementType>{ r + o.r, g + o.g, b + o.b };
				return *this;
			}
			constexpr RGB<ElementType>& operator += (const R<ElementType>& r) {
				this->r += r;
				return *this;
			}
			constexpr RGB<ElementType>& operator += (const G<ElementType>& g) {
				this->g += g;
				return *this;
			}
			constexpr RGB<ElementType>& operator += (const B<ElementType>& b) {
				this->b += b;
				return *this;
			}
			constexpr RGB<ElementType>& operator -= (const RGB<ElementType>& o) {
				*this = RGB<ElementType>{ r - o.r, g - o.g, b - o.b };
				return *this;
			}
			constexpr RGB<ElementType>& operator -= (const R<ElementType>& r) {
				this->r -= r;
				return *this;
			}
			constexpr RGB<ElementType>& operator -= (const G<ElementType>& g) {
				this->g -= g;
				return *this;
			}
			constexpr RGB<ElementType>& operator -= (const B<ElementType>& b) {
				this->b -= b;
				return *this;
			}

			template<typename H  = float, typename SV = H>
			constexpr HSV<H, SV> hsv() {
				using FloatType = std::conditional_t<
					std::is_floating_point_v<ElementType>, ElementType, double>; // 浮動小数点型を確保
				using ChannelType = ElementType;

				FloatType r = static_cast<FloatType>(this->r.value);
				FloatType g = static_cast<FloatType>(this->g.value);
				FloatType b = static_cast<FloatType>(this->b.value);

				if constexpr (std::is_integral_v<ChannelType>) {
					r /= std::numeric_limits<ChannelType>::max();
					g /= std::numeric_limits<ChannelType>::max();
					b /= std::numeric_limits<ChannelType>::max();
				}

				FloatType max_val = std::max({ r, g, b });
				FloatType min_val = std::min({ r, g, b });
				FloatType delta = max_val - min_val;

				FloatType h = 0;
				FloatType s = (max_val == 0) ? 0 : (delta / max_val);
				FloatType v = max_val;

				if (delta != 0) {
					if (max_val == r) {
						h = 60 * (std::fmod(((g - b) / delta), 6));
					}
					else if (max_val == g) {
						h = 60 * (((b - r) / delta) + 2);
					}
					else {
						h = 60 * (((r - g) / delta) + 4);
					}
				}
				if (h < 0) h += 360;

				if constexpr (std::is_integral_v<H>) {
					h = std::round(h);
				}
				if constexpr (std::is_integral_v<SV>) {
					s = std::round(s * std::numeric_limits<SV>::max());
					v = std::round(v * std::numeric_limits<SV>::max());
				}

				return { static_cast<H>(h), static_cast<SV>(s), static_cast<SV>(v) };
			}
		};

		template<typename ElementType>
		struct RGBA : public RGB<ElementType> {
			using element_type = ElementType;
			static constexpr VSL_NAMESPACE::data_format::___Format graphic_type
				= VSL_NAMESPACE::data_format::convert_graphic_type<ElementType>().template toVec<4>();

			A<ElementType> a;

			constexpr RGBA() = default;
			constexpr RGBA(const ElementType& r, const ElementType& g, const ElementType& b, const ElementType& a) 
				: RGB<ElementType>(r, g, b), a(a) {}

			/*
			* alpha値に影響を与えません
			*/
			constexpr RGBA<ElementType> operator * (const ElementType& m) const {
				return { this->r * m, this->g * m, this->b * m, this->a };
			}
			/*
			* alpha値に影響を与えません
			*/
			constexpr RGBA<ElementType> operator / (const ElementType& m) const {
				return { this->r / m, this->g / m, this->b / m, this->a };
			}
			/*
			* alpha値に影響を与えません
			*/
			constexpr RGBA<ElementType>& operator *= (const ElementType& m) {
				this->r *= m;
				this->g *= m;
				this->b *= m;
				return *this;
			}
			/*
			* alpha値に影響を与えません
			*/
			constexpr RGBA<ElementType>& operator /= (const ElementType& m) {
				this->r /= m;
				this->g /= m;
				this->b /= m;
				return *this;
			}

			// =================================================================

			constexpr RGBA<ElementType> operator + (const RGB<ElementType>& o) const {
				return { this->r + o.r, this->g + o.g, this->b + o.b, this->a };
			}
			constexpr RGBA<ElementType> operator + (const RGBA<ElementType>& o) const {
				return { this->r + o.r, this->g + o.g, this->b + o.b, this->a + o.a };
			}
			constexpr RGBA<ElementType> operator + (const R<ElementType>& r) const {
				return { this->r + r, this->g, this->b, this->a };
			}
			constexpr RGBA<ElementType> operator + (const G<ElementType>& g) const {
				return { this->r, this->g + g, this->b, this->a };
			}
			constexpr RGBA<ElementType> operator + (const B<ElementType>& b) const {
				return { this->r, this->g, this->b + b, this->a };
			}
			constexpr RGBA<ElementType> operator + (const A<ElementType>& a) const {
				return { this->r, this->g, this->b, this->a + a };
			}

			constexpr RGBA<ElementType> operator - (const RGB<ElementType>& o) const {
				return { this->r - o.r, this->g - o.g, this->b - o.b, this->a };
			}
			constexpr RGBA<ElementType> operator - (const RGBA<ElementType>& o) const {
				return { this->r - o.r, this->g - o.g, this->b - o.b, this->a - o.a };
			}
			constexpr RGBA<ElementType> operator - (const R<ElementType>& r) const {
				return { this->r - r, this->g, this->b, this->a };
			}
			constexpr RGBA<ElementType> operator - (const G<ElementType>& g) const {
				return { this->r, this->g - g, this->b, this->a };
			}
			constexpr RGBA<ElementType> operator - (const B<ElementType>& b) const {
				return { this->r, this->g, this->b - b, this->a };
			}
			constexpr RGBA<ElementType> operator - (const A<ElementType>& a) const {
				return { this->r, this->g, this->b, this->a - a };
			}

			constexpr RGBA<ElementType>& operator += (const RGB<ElementType>& o) {
				*this = RGBA<ElementType>{ this->r + o.r, this->g + o.g, this->b + o.b, this->a };
				return *this;
			}
			constexpr RGBA<ElementType>& operator += (const RGBA<ElementType>& o) {
				*this = RGBA<ElementType>{ this->r + o.r, this->g + o.g, this->b + o.b, this->a + o.a };
				return *this;
			}
			constexpr RGBA<ElementType>& operator += (const R<ElementType>& r) {
				this->r += r;
				return *this;
			}
			constexpr RGBA<ElementType>& operator += (const G<ElementType>& g) {
				this->g += g;
				return *this;
			}
			constexpr RGBA<ElementType>& operator += (const B<ElementType>& b) {
				this->b += b;
				return *this;
			}
			constexpr RGBA<ElementType>& operator += (const A<ElementType>& a) {
				this->a += a;
				return *this;
			}
			constexpr RGBA<ElementType>& operator -= (const RGB<ElementType>& o) {
				*this = RGBA<ElementType>{ this->r - o.r, this->g - o.g, this->b - o.b, this->a };
				return *this;
			}
			constexpr RGBA<ElementType>& operator -= (const RGBA<ElementType>& o) {
				*this = RGBA<ElementType>{ this->r - o.r, this->g - o.g, this->b - o.b, this->a - o.a };
				return *this;
			}
			constexpr RGBA<ElementType>& operator -= (const R<ElementType>& r) {
				this->r -= r;
				return *this;
			}
			constexpr RGBA<ElementType>& operator -= (const G<ElementType>& g) {
				this->g -= g;
				return *this;
			}
			constexpr RGBA<ElementType>& operator -= (const B<ElementType>& b) {
				this->b -= b;
				return *this;
			}
			constexpr RGBA<ElementType>& operator -= (const A<ElementType>& a) {
				this->a -= a;
				return *this;
			}

			template<typename H = float, typename SV = H>
			constexpr HSV<H, SV> hsva() {
				using FloatType = std::conditional_t<
					std::is_floating_point_v<ElementType>, ElementType, double>; // 浮動小数点型を確保
				using ChannelType = ElementType;

				FloatType a = static_cast<FloatType>(this->a.value);

				if constexpr (std::is_integral_v<ElementType>)
					a /= std::numeric_limits<ElementType>::max();

				if constexpr (std::is_integral_v<SV>)
					a = std::round(a * std::numeric_limits<SV>::max());

				return this->hsv() + A<SV>(static_cast<SV>(a));
			}
		};

		template<typename HueType, typename SaturationAndValueType>
		struct HSV {
			using element_type = HueType;
			using hue_type = HueType;
			using saturation_type = SaturationAndValueType;
			using value_type = SaturationAndValueType;
			static constexpr VSL_NAMESPACE::data_format::___Format graphic_type
				= VSL_NAMESPACE::data_format::convert_graphic_type<HueType>().template toVec<3>();

			H<hue_type> h;
			S<saturation_type> s;
			V<value_type> v;

			constexpr HSV() = default;
			constexpr HSV(const HueType& h, const SaturationAndValueType& s, const SaturationAndValueType& v) : h(h), s(s), v(v){}

			constexpr HSV<HueType, SaturationAndValueType> operator + (const HSV<HueType, SaturationAndValueType>& o) const {
				return { this->h + o.h, this->s + o.s, this->v + o.v };
			}
			constexpr HSVA<HueType, SaturationAndValueType> operator + (const HSVA<HueType, SaturationAndValueType>& o) const {
				return { this->h + o.h, this->s + o.s, this->v + o.v, o.a };
			}
			constexpr HSV<HueType, SaturationAndValueType> operator + (const H<HueType>& h) const {
				return { this->h + h, this->s, this->v };
			}
			constexpr HSV<HueType, SaturationAndValueType> operator + (const S<saturation_type>& s) const {
				return { this->h, this->s + s, this->v };
			}
			constexpr HSV<HueType, SaturationAndValueType> operator + (const V<value_type>& v) const {
				return { this->h, this->s, this->v + v };
			}
			constexpr HSVA<HueType, SaturationAndValueType> operator + (const A<SaturationAndValueType>& a) const {
				return { this->h, this->s, this->v, a };
			}

			constexpr HSV<HueType, SaturationAndValueType> operator - (const HSV<HueType, SaturationAndValueType>& o) const {
				return { this->h - o.h, this->s - o.s, this->v - o.v };
			}
			constexpr HSV<HueType, SaturationAndValueType> operator - (const H<HueType>& h) const {
				return { this->h - h, this->s, this->v };
			}
			constexpr HSV<HueType, SaturationAndValueType> operator - (const S<saturation_type>& s) const {
				return { this->h, this->s - s, this->v };
			}
			constexpr HSV<HueType, SaturationAndValueType> operator - (const V<value_type>& v) const {
				return { this->h, this->s, this->v - v };
			}
			constexpr HSVA<HueType, SaturationAndValueType> operator - (const A<SaturationAndValueType>& a) const {
				return { this->h, this->s, this->v, a };
			}

			constexpr HSV<HueType, SaturationAndValueType>& operator += (const HSV<HueType, SaturationAndValueType>& o) {
				*this = HSV<HueType, SaturationAndValueType>{ h + o.h, s + o.s, v + o.v };
				return *this;
			}
			constexpr HSV<HueType, SaturationAndValueType>& operator += (const H<HueType>& h) {
				this->h += h;
				return *this;
			}
			constexpr HSV<HueType, SaturationAndValueType>& operator += (const S<saturation_type>& s) {
				this->s += s;
				return *this;
			}
			constexpr HSV<HueType, SaturationAndValueType>& operator += (const V<value_type>& v) {
				this->v += v;
				return *this;
			}
			constexpr HSV<HueType, SaturationAndValueType>& operator -= (const HSV<HueType, SaturationAndValueType>& o) {
				*this = HSV<HueType, SaturationAndValueType>{ h - o.h, s - o.s, v - o.v };
				return *this;
			}
			constexpr HSV<HueType, SaturationAndValueType>& operator -= (const H<HueType>& h) {
				this->h -= h;
				return *this;
			}
			constexpr HSV<HueType, SaturationAndValueType>& operator -= (const S<saturation_type>& s) {
				this->s -= s;
				return *this;
			}
			constexpr HSV<HueType, SaturationAndValueType>& operator -= (const V<value_type>& v) {
				this->v -= v;
				return *this;
			}

			template<typename T = float>
			constexpr RGB<T> rgb() {
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
					r = c; g = x; b = 0;
				}
				else if (h < 120) {
					r = x; g = c; b = 0;
				}
				else if (h < 180) {
					r = 0; g = c; b = x;
				}
				else if (h < 240) {
					r = 0; g = x; b = c;
				}
				else if (h < 300) {
					r = x; g = 0; b = c;
				}
				else {
					r = c; g = 0; b = x;
				}

				r += m;
				g += m;
				b += m;

				if constexpr (std::is_integral_v<T>) {
					constexpr T max_val = std::numeric_limits<T>::max();
					return { static_cast<T>(std::round(r * max_val)),
							 static_cast<T>(std::round(g * max_val)),
							 static_cast<T>(std::round(b * max_val)) };
				}
				else {
					return { static_cast<T>(r), static_cast<T>(g), static_cast<T>(b) };
				}
			}
		};

		template<typename HueType, typename SaturationAndValueType>
		struct HSVA : public HSV<HueType, SaturationAndValueType> {
			using element_type = HueType;
			using hue_type = HueType;
			using saturation_type = SaturationAndValueType;
			using value_type = SaturationAndValueType;
			using alpha_type = SaturationAndValueType;
			static constexpr VSL_NAMESPACE::data_format::___Format graphic_type
				= VSL_NAMESPACE::data_format::convert_graphic_type<HueType>().template toVec<4>();

			A<alpha_type> a;

			constexpr HSVA() = default;
			constexpr HSVA(const HueType& h, const SaturationAndValueType& s, const SaturationAndValueType& v, const SaturationAndValueType& a) 
				: HSV<HueType, SaturationAndValueType>(h, s, v), a(a) {}

			constexpr HSVA<HueType, SaturationAndValueType> operator + (const HSV<HueType, SaturationAndValueType>& o) const {
				return { this->h + o.h, this->s + o.s, this->v + o.v, a };
			}
			constexpr HSVA<HueType, SaturationAndValueType> operator + (const HSVA<HueType, SaturationAndValueType>& o) const {
				return { this->h + o.h, this->s + o.s, this->v + o.v, a + o.a };
			}
			constexpr HSVA<HueType, SaturationAndValueType> operator + (const H<HueType>& h) const {
				return { this->h + h, this->s, this->v, a };
			}
			constexpr HSVA<HueType, SaturationAndValueType> operator + (const S<saturation_type>& s) const {
				return { this->h, this->s + s, this->v, a };
			}
			constexpr HSVA<HueType, SaturationAndValueType> operator + (const V<value_type>& v) const {
				return { this->h, this->s, this->v + v, a };
			}
			constexpr HSVA<HueType, SaturationAndValueType> operator + (const A<alpha_type>& a) const {
				return { this->h, this->s, this->v, this->a + a };
			}

			constexpr HSVA<HueType, SaturationAndValueType> operator - (const HSV<HueType, SaturationAndValueType>& o) const {
				return { this->h - o.h, this->s - o.s, this->v - o.v, a };
			}
			constexpr HSVA<HueType, SaturationAndValueType> operator - (const HSVA<HueType, SaturationAndValueType>& o) const {
				return { this->h - o.h, this->s - o.s, this->v - o.v, a - o.a };
			}
			constexpr HSVA<HueType, SaturationAndValueType> operator - (const H<HueType>& h) const {
				return { this->h - h, this->s, this->v, a };
			}
			constexpr HSVA<HueType, SaturationAndValueType> operator - (const S<saturation_type>& s) const {
				return { this->h, this->s - s, this->v, a };
			}
			constexpr HSVA<HueType, SaturationAndValueType> operator - (const V<value_type>& v) const {
				return { this->h, this->s, this->v - v, a };
			}
			constexpr HSVA<HueType, SaturationAndValueType> operator - (const A<alpha_type>& a) const {
				return { this->h, this->s, this->v, this->a - a };
			}

			constexpr HSVA<HueType, SaturationAndValueType>& operator += (const HSV<HueType, SaturationAndValueType>& o) {
				*this = HSVA<HueType, SaturationAndValueType>{ this->h + o.h, this->s + o.s, this->v + o.v, a };
				return *this;
			}
			constexpr HSVA<HueType, SaturationAndValueType>& operator += (const HSVA<HueType, SaturationAndValueType>& o) {
				*this = HSVA<HueType, SaturationAndValueType>{ this->h + o.h, this->s + o.s, this->v + o.v, a + o.a };
				return *this;
			}
			constexpr HSVA<HueType, SaturationAndValueType>& operator += (const H<HueType>& h) {
				this->h += h;
				return *this;
			}
			constexpr HSVA<HueType, SaturationAndValueType>& operator += (const S<saturation_type>& s) {
				this->s += s;
				return *this;
			}
			constexpr HSVA<HueType, SaturationAndValueType>& operator += (const V<value_type>& v) {
				this->v += v;
				return *this;
			}
			constexpr HSVA<HueType, SaturationAndValueType>& operator += (const A<alpha_type>& a) {
				this->a += a;
				return *this;
			}
			constexpr HSVA<HueType, SaturationAndValueType>& operator -= (const HSV<HueType, SaturationAndValueType>& o) {
				*this = HSVA<HueType, SaturationAndValueType>{ this->h - o.h, this->s - o.s, this->v - o.v, a };
				return *this;
			}
			constexpr HSVA<HueType, SaturationAndValueType>& operator -= (const HSVA<HueType, SaturationAndValueType>& o) {
				*this = HSVA<HueType, SaturationAndValueType>{ this->h - o.h, this->s - o.s, this->v - o.v, a - o.a };
				return *this;
			}
			constexpr HSVA<HueType, SaturationAndValueType>& operator -= (const H<HueType>& h) {
				this->h -= h;
				return *this;
			}
			constexpr HSVA<HueType, SaturationAndValueType>& operator -= (const S<saturation_type>& s) {
				this->s -= s;
				return *this;
			}
			constexpr HSVA<HueType, SaturationAndValueType>& operator -= (const V<value_type>& v) {
				this->v -= v;
				return *this;
			}
			constexpr HSVA<HueType, SaturationAndValueType>& operator -= (const A<alpha_type>& a) {
				this->a -= a;
				return *this;
			}

			template<typename T = float>
			constexpr RGBA<T> rgba() {
				using FloatType = std::conditional_t<
					std::is_floating_point_v<SaturationAndValueType>, SaturationAndValueType, double>; // 浮動小数点型を確保
				using ChannelType = T;

				FloatType a = static_cast<FloatType>(this->a.value);

				if constexpr (std::is_integral_v<SaturationAndValueType>) {
					a /= std::numeric_limits<SaturationAndValueType>::max();
				}

				if constexpr (std::is_integral_v<T>) {
					return this->rgb() + A<ChannelType>(static_cast<ChannelType>(a * std::numeric_limits<ChannelType>::max()));
				}
				else {
					return this->rgb() + A<ChannelType>(static_cast<ChannelType>(a));
				}
			}
		};

		template<typename T>
		std::ostream& operator <<(std::ostream& out, const RGB<T>& rgb) {
			out << "(r, g, b)=" << "(" << rgb.r.value << ", " << rgb.g.value
				<< ", " << rgb.b.value << ")";
			return out;
		};

		template<typename T>
		std::ostream& operator <<(std::ostream& out, const RGBA<T>& rgb) {
			out << "(r, g, b, a)=" << "(" << rgb.r.value << ", " << rgb.g.value
				<< ", " << rgb.b.value << ", " << rgb.a.value << ")";
			return out;
		};

		template<typename T>
		std::ostream& operator <<(std::ostream& out, const HSV<T>& rgb) {
			out << "(h, s, v)=" << "(" << rgb.h.value << ", " << rgb.s.value
				<< ", " << rgb.v.value << ")";
			return out;
		};

		template<typename T>
		std::ostream& operator <<(std::ostream& out, const HSVA<T>& rgb) {
			out << "(h, s, v, a)=" << "(" << rgb.h.value << ", " << rgb.s.value
				<< ", " << rgb.v.value << ", " << rgb.a.value << ")";
			return out;
		};

		template <concepts::is_rgb_elemenet L, concepts::is_rgb_elemenet R>
			requires (!std::convertible_to<L, R>) && std::same_as<typename L::element_type, typename R::element_type>
		RGB<typename L::element_type> operator +(const L& l, const R& r) {
			RGB<typename L::element_type> rgb;
			rgb += l;
			rgb += r;
			return rgb;
		}

		template <concepts::is_hsv_elemenet L, concepts::is_hsv_elemenet R>
			requires (!std::convertible_to<L, R>) && std::same_as<typename L::element_type, typename R::element_type>
		HSV<typename L::element_type> operator +(const L& l, const R& r) {
			HSV<typename L::element_type> hsv;
			hsv += l;
			hsv += r;
			return hsv;
		}

		template <concepts::is_rgb_elemenet L, typename ElementType>
			requires std::same_as<typename L::element_type, ElementType>
		RGBA<typename L::element_type> operator +(const L& l, A<ElementType> a) {
			RGBA<typename L::element_type> rgb;
			rgb += l;
			rgb += a;
			return rgb;
		}

		template <concepts::is_hsv_elemenet L, typename ElementType>
			requires std::same_as<typename L::element_type, ElementType>
		HSVA<typename L::element_type> operator +(const L& l, A<ElementType> a) {
			HSVA<typename L::element_type> hsv;
			hsv += l;
			hsv += a;
			return hsv;
		}

		template <typename ElementType, concepts::is_rgb_elemenet R>
			requires std::same_as<typename R::element_type, ElementType>
		RGBA<typename R::element_type> operator +(A<ElementType> a, const R& r) {
			RGBA<typename R::element_type> rgb;
			rgb += r;
			rgb += a;
			return rgb;
		}

		template <typename ElementType, concepts::is_hsv_elemenet R>
			requires std::same_as<typename R::element_type, ElementType>
		HSVA<typename R::element_type> operator +(A<ElementType> a, const R& r) {
			HSVA<typename R::element_type> rgb;
			rgb += r;
			rgb += a;
			return rgb;
		}

		template <concepts::is_rgb_elemenet L, concepts::is_rgb_elemenet R>
			requires (!std::convertible_to<L, R>) && std::same_as<typename L::element_type, typename R::element_type>
		RGB<typename L::element_type> operator -(const L& l, const R& r) {
			RGB<typename L::element_type> rgb;
			rgb -= l;
			rgb -= r;
			return rgb;
		}

		template <concepts::is_hsv_elemenet L, concepts::is_hsv_elemenet R>
			requires (!std::convertible_to<L, R>) && std::same_as<typename L::element_type, typename R::element_type>
		HSV<typename L::element_type> operator -(const L& l, const R& r) {
			HSV<typename L::element_type> hsv;
			hsv -= l;
			hsv -= r;
			return hsv;
		}

		template <concepts::is_rgb_elemenet L, typename ElementType>
			requires std::same_as<typename L::element_type, ElementType>
		RGBA<typename L::element_type> operator -(const L& l, const A<ElementType>& a) {
			RGBA<typename L::element_type> rgb;
			rgb -= l;
			rgb -= a;
			return rgb;
		}

		template <concepts::is_hsv_elemenet L, typename ElementType>
			requires std::same_as<typename L::element_type, ElementType>
		HSVA<typename L::element_type> operator -(const L& l, const A<ElementType>& a) {
			HSVA<typename L::element_type> hsv;
			hsv -= l;
			hsv -= a;
			return hsv;
		}

		template <typename ElementType, concepts::is_rgb_elemenet R>
			requires std::same_as<typename R::element_type, ElementType>
		RGBA<typename R::element_type> operator -(const A<ElementType>& a, const R& r) {
			RGBA<typename R::element_type> rgb;
			rgb -= r;
			rgb -= a;
			return rgb;
		}

		template <typename ElementType, concepts::is_hsv_elemenet R>
			requires std::same_as<typename R::element_type, ElementType>
		HSVA<typename R::element_type> operator -(const A<ElementType>& a, const R& r) {
			HSVA<typename R::element_type> rgb;
			rgb -= r;
			rgb -= a;
			return rgb;
		}

		namespace literals {
#define LITERAL_EXPANDER(Upper, Lower) \
		constexpr Upper<double> operator AHO_LITERAL(Lower)(long double v) { return { (double)v }; }; \
		constexpr Upper<int> operator AHO_LITERAL(Lower)(unsigned long long v) { return { (int)v }; }; \
		constexpr Upper<float> operator AHO_LITERAL(f_ ## Lower)(long double v) { return { (float)v }; }; \
		constexpr Upper<size_t> operator AHO_LITERAL(l_ ## Lower)(unsigned long long v) { return { v }; }; \
		constexpr Upper<double> Lower(1.0);		

			LITERAL_EXPANDER(R, r);
			LITERAL_EXPANDER(G, g);
			LITERAL_EXPANDER(B, b);
			LITERAL_EXPANDER(H, h);
			LITERAL_EXPANDER(S, s);
			LITERAL_EXPANDER(V, v);
			LITERAL_EXPANDER(A, a);
		}
	}

	using RGB = color::RGB<float>;
	using RGBA = color::RGBA<float>;
	using HSV = color::HSV<float, float>;
	using HSVA = color::HSVA<float, float>;

	namespace color {
		using IntRGB = color::RGB<int8_t>;
		using IntRGBA = color::RGBA<int8_t>;
		using FloatRGB = color::RGB<float>;
		using FloatRGBA = color::RGBA<float>;

		using IntHSV = color::HSV<short, int8_t>;
		using IntHSVA = color::HSVA<short, int8_t>;
		using FloatHSV = color::HSV<float, float>;
		using FloatHSVA = color::HSVA<float, float>;
	}

	namespace concepts {
		using namespace color::concepts;
	}

	namespace literals {
		using namespace color::literals;
	}
}

#undef RGB_ELEMENT_TYPE_DEFINITION_EXPANDER
#undef HSV_ELEMENT_TYPE_DEFINITION_EXPANDER
#undef LITERAL_EXPANDER