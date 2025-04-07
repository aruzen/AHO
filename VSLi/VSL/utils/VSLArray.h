#pragma once
#include "../define.h"

namespace VSL_NAMESPACE {
	template<typename T>
	concept convert_to_vsl_array = requires {
		typename T::element_type;
	};

	template<convert_to_vsl_array T>
	constexpr size_t vsl_array_size() {
		return sizeof(T) / sizeof(T::element_type);
	}

	template<typename T>
	concept has_point_index = requires {
		{ T::point_index } -> std::same_as<size_t>;
	};

	template<typename T>
	concept has_size_index = requires {
		{ T::size_index } -> std::same_as<size_t>;
	};

	template<typename T>
	concept has_rgb_color_index = requires {
		{ T::color_index } -> std::same_as<size_t>;
	};

	template<typename T, size_t Size>
	concept convert_to_vsl_sized_array = convert_to_vsl_array<T> && Size == vsl_array_size<T>();

	template<typename T, size_t Size>
	concept convert_to_vsl_at_least_array = convert_to_vsl_array<T> && Size <= vsl_array_size<T>();

	template<convert_to_vsl_array T, typename ElementType = T::element_type, size_t Size = vsl_array_size<T>()>
	struct __VSLArray {
		using element_type = ElementType;

		std::array<ElementType, Size>* __ptr;

		constexpr __VSLArray(T* t) {
			__ptr = reinterpret_cast<std::array<ElementType, Size>*>(t);
		}

		ElementType& operator[](size_t t) {
			return (*__ptr)[t];
		}
	};

	template <typename ElementType>
	struct __VSLD2PointDefault {
		using element_type = ElementType;
		ElementType x, y;
	};

	template <typename ElementType>
	struct __VSLD2PointAccessor {
		using element_type = ElementType;
		ElementType& x, & y;
	};

	template<typename T>
	struct __VSLD2Point {
		static constexpr int SPECIALIZED_ID = 0;
	};

	template<typename T>
	concept is_point = requires (T t) {
		typename T::element_type;
		{ t.x } -> std::convertible_to<typename T::element_type>;
		{ t.y } -> std::convertible_to<typename T::element_type>;
	};

	template <typename T>
		requires is_point<T>
	struct __VSLD2Point<T> : public __VSLD2PointAccessor<typename T::element_type> {
		static constexpr int SPECIALIZED_ID = 1;

		using element_type = T::element_type;

		__VSLD2Point(T* t) : __VSLD2PointAccessor<typename T::element_type>{ t->x, t->y } {
			if constexpr (VSL_NAMESPACE::validation) {
				VSL_NAMESPACE::loggingln(typeid(*this).name(), " : ", typeid(T).name(), " (processed by is_point)");
			}
		}
	};

	template <typename T>
		requires convert_to_vsl_at_least_array<T, 2> && (!has_point_index<T>) && (!is_point<T>)
	struct __VSLD2Point<T> : public __VSLArray<T, typename T::element_type, vsl_array_size<T>()>, public __VSLD2PointAccessor<typename T::element_type> {
		static constexpr int SPECIALIZED_ID = 2;

		using element_type = T::element_type;

		__VSLD2Point(T* t) : __VSLArray<T, element_type, vsl_array_size<T>()>(t), __VSLD2PointAccessor<typename T::element_type>{ (*this)[0], (*this)[1] } {
			if constexpr (VSL_NAMESPACE::validation) {
				VSL_NAMESPACE::loggingln(typeid(*this).name(), " : ", typeid(T).name(), " (processed by vsl_array)");
			}
		}
	};

	template <typename T>
		requires convert_to_vsl_at_least_array<T, 2>&& has_point_index<T> && (!is_point<T>)
	struct __VSLD2Point<T> : public __VSLArray<T, typename T::element_type, vsl_array_size<T>()>, public __VSLD2PointAccessor<typename T::element_type> {
		static constexpr int SPECIALIZED_ID = 3;

		using element_type = T::element_type;

		__VSLD2Point(T* t) : __VSLArray<T, element_type, vsl_array_size<T>()>(t),
			__VSLD2PointAccessor<typename T::element_type>{ (*this)[T::point_index], (*this)[T::point_index + 1]} {
			if constexpr (VSL_NAMESPACE::validation) {
				VSL_NAMESPACE::loggingln(typeid(*this).name(), " : ", typeid(T).name(), " (processed by vsl_array & has_index)");
			}
		}
	};

	template <typename ElementType>
	struct __VSLD2SizeDefault {
		using element_type = ElementType;
		ElementType width, height;
	};

	template <typename ElementType>
	struct __VSLD2SizeAccessor {
		using element_type = ElementType;
		ElementType& width, & height;
	};

	template<typename T>
	struct __VSLD2Size {};

	template<typename T>
	concept is_size = requires (T t) {
		typename T::element_type;
		{ t.height } -> std::same_as<typename T::element_type>;
		{ t.width } -> std::same_as<typename T::element_type>;
	};

	template<typename T>
		requires is_size<T>
	struct __VSLD2Size<T> : public __VSLD2SizeAccessor<typename T::element_type> {
		static constexpr int SPECIALIZED_ID = 1;
		using element_type = T::element_type;

		__VSLD2Size(T* t) : __VSLD2SizeAccessor<typename T::element_type>{ t->width, t->height } {}
	};

	template <typename T>
		requires convert_to_vsl_at_least_array<T, 2> && (!has_size_index<T>) && (!is_size<T>)
	struct __VSLD2Size<T> : public __VSLD2SizeAccessor<typename T::element_type>, public __VSLArray<T, typename T::element_type, vsl_array_size<T>()> {
		static constexpr int SPECIALIZED_ID = 2;

		using element_type = T::element_type;

		__VSLD2Size(T* t) : __VSLArray<T, element_type, vsl_array_size<T>()>(t),
			__VSLD2SizeAccessor<typename T::element_type>{ (*this)[0], (*this)[1] } {
			if constexpr (VSL_NAMESPACE::validation) {
				VSL_NAMESPACE::loggingln(typeid(*this).name(), " : ", typeid(T).name(), " (processed by vsl_array)");
			}
		}
	};

	template <typename T>
		requires convert_to_vsl_at_least_array<T, 2>&& has_size_index<T> && (!is_size<T>)
	struct __VSLD2Size<T> : public __VSLD2SizeAccessor<typename T::element_type>, public __VSLArray<T, typename T::element_type, vsl_array_size<T>()> {
		static constexpr int SPECIALIZED_ID = 3;

		using element_type = T::element_type;

		__VSLD2Size(T* t) : __VSLArray<T, element_type, vsl_array_size<T>()>(t),
			__VSLD2SizeAccessor<typename T::element_type>{ (*this)[T::size_index], (*this)[T::size_index + 1] } {
			if constexpr (VSL_NAMESPACE::validation) {
				VSL_NAMESPACE::loggingln(typeid(*this).name(), " : ", typeid(T).name(), " (processed by vsl_array & has_index)");
			}
		}
	};

	template <typename ElementType>
	struct __VSLD2RectangleDefault : public __VSLD2PointDefault<ElementType>, public __VSLD2SizeDefault<ElementType> {
		using element_type = ElementType;
		static constexpr size_t size_index = 2;
	};

	template <typename ElementType>
	struct __VSLD2RectangleAccessor : public __VSLD2PointAccessor<ElementType>, public __VSLD2SizeAccessor<ElementType> {
		using element_type = ElementType;
	};

	template<typename T>
	struct __VSLD2Rectangle {};

	template<typename T>
	concept is_rectangle = is_point<T> && is_size<T>;

	template<typename T>
		requires is_rectangle<T>
	struct __VSLD2Rectangle<T> : public __VSLD2RectangleAccessor<typename T::element_type> {

		__VSLD2Rectangle(T* t) : __VSLD2RectangleAccessor<typename T::element_type>{ t->x, t->y, t->width, t->height } {}
	};

	template <typename T>
		requires convert_to_vsl_at_least_array<T, 4>
	struct __VSLD2Rectangle<T> : public __VSLD2RectangleAccessor<typename T::element_type>, public __VSLArray<T, typename T::element_type, vsl_array_size<T>()> {
		using element_type = T::element_type;

		__VSLD2Rectangle(T* t) : __VSLArray<T, element_type, vsl_array_size<T>()>(t),
			__VSLD2RectangleAccessor<typename T::element_type>{
				has_point_index<T> ? (*this)[T::point_index] : (*this)[0],
				has_point_index<T> ? (*this)[T::point_index + 1] : (*this)[1],
				has_size_index<T> ? (*this)[T::size_index] : (*this)[2],
				has_size_index<T> ? (*this)[T::size_index + 1] : (*this)[3] } {}
	};

	template<typename ElementType>
	struct __VSLDRGBColorDefault {
		using element_type = ElementType;

		element_type r, g, b;
	};

	template<typename ElementType>
	struct __VSLDRGBColorAccessor {
		using element_type = ElementType;

		element_type& r, & g, & b;
	};

	template<typename T>
	struct __VSLDRGBColor {};

	template<typename T>
	concept is_rgb = requires (T t) {
		typename T::element_type;
		{ t.r } -> std::same_as<typename T::element_type>;
		{ t.g } -> std::same_as<typename T::element_type>;
		{ t.b } -> std::same_as<typename T::element_type>;
	};

	template<typename T>
		requires is_rgb<T>
	struct __VSLDRGBColor<T> : public __VSLDRGBColorAccessor<typename T::element_type> {
		using element_type = T::element_type;

		__VSLDRGBColor(T* t) : __VSLDRGBColorAccessor<typename T::element_type>{ t->r, t->g, t->b } {}
	};

	template <typename T>
		requires convert_to_vsl_at_least_array<T, 3> && (!has_rgb_color_index<T>) && (!is_rgb<T>)
	struct __VSLDRGBColor<T>
		: public __VSLDRGBColorAccessor<typename T::element_type>
		, public __VSLArray<T, typename T::element_type, vsl_array_size<T>()> {

		using element_type = T::element_type;

		__VSLDRGBColor(T* t) : __VSLArray<T, element_type, vsl_array_size<T>()>(t),
			__VSLDRGBColorAccessor<typename T::element_type>{ (*this)[0], (*this)[1], (*this)[2] } {}
	};

	template <typename T>
		requires convert_to_vsl_at_least_array<T, 3>&& has_rgb_color_index<T> && (!is_rgb<T>)
	struct __VSLDRGBColor<T>
		: public __VSLDRGBColorAccessor<typename T::element_type>
		, public __VSLArray<T, typename T::element_type, vsl_array_size<T>()> {

		using element_type = T::element_type;

		__VSLDRGBColor(T* t) : __VSLArray<T, element_type, vsl_array_size<T>()>(t),
			__VSLDRGBColorAccessor<typename T::element_type>{ (*this)[T::color_index + 0], (*this)[T::color_index + 1], (*this)[T::color_index + 2] } {}
	};
}