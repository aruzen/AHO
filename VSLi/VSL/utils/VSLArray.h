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
	concept has_element_index = requires {
		{ T::element_index } -> std::same_as<size_t>;
	};

	template<typename T>
	concept has_size_index = requires {
		{ T::size_index } -> std::same_as<size_t>;
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
	struct __VSLD2Point<T> {
		static constexpr int SPECIALIZED_ID = 1;

		using element_type = T::element_type;
		element_type& x, & y;

		__VSLD2Point(T* t) : x(t->x), y(t->y) {
			if constexpr (VSL_NAMESPACE::validation) {
				VSL_NAMESPACE::loggingln(typeid(*this).name(), " : ", typeid(T).name(), " (processed by is_point)");
			}
		}
	};

	template <typename T>
		requires convert_to_vsl_at_least_array<T, 2> && (!is_point<T>)
	struct __VSLD2Point<T> : __VSLArray<T, typename T::element_type, vsl_array_size<T>()> {
		static constexpr int SPECIALIZED_ID = 2;

		using element_type = T::element_type;
		element_type& x, & y;

		__VSLD2Point(T* t) : __VSLArray<T, element_type, vsl_array_size<T>()>(t), x((*this)[0]), y((*this)[1]) {
			if constexpr (VSL_NAMESPACE::validation) {
				VSL_NAMESPACE::loggingln(typeid(*this).name(), " : ", typeid(T).name(), " (processed by vsl_array)");
			}
		}
	};

	template <typename T>
		requires convert_to_vsl_at_least_array<T, 2>&& has_element_index<T> && (!is_point<T>)
	struct __VSLD2Point<T> : __VSLArray<T, typename T::element_type, vsl_array_size<T>()> {
		static constexpr int SPECIALIZED_ID = 3;

		using element_type = T::element_type;
		element_type& x, & y;

		__VSLD2Point(T* t) : __VSLArray<T, element_type, vsl_array_size<T>()>(t),
			x((*this)[T::element_index]),
			y((*this)[T::element_index + 1]) {
			if constexpr (VSL_NAMESPACE::validation) {
				VSL_NAMESPACE::loggingln(typeid(*this).name(), " : ", typeid(T).name(), " (processed by vsl_array & has_index)");
			}
		}
	};

	template <typename ElementType>
	struct __VSLD2SizeDefault {
		using element_type = ElementType;
		ElementType height, weight;
	};

	template<typename T>
	struct __VSLD2Size {};

	template<typename T>
	concept is_size = requires (T t) {
		typename T::element_type;
		{ t.height } -> std::same_as<typename T::element_type>;
		{ t.weight } -> std::same_as<typename T::element_type>;
	};

	template<typename T>
		requires is_size<T>
	struct __VSLD2Size<T> {
		using element_type = T::element_type;
		element_type& height, & weight;

		__VSLD2Size(T* t) {
			height = t->height;
			weight = t->weight;
		}
	};

	template <typename T>
		requires convert_to_vsl_at_least_array<T, 2>
	struct __VSLD2Size<T> : __VSLArray<T, typename T::element_type, vsl_array_size<T>()> {
		using element_type = T::element_type;
		element_type& height, & weight;

		__VSLD2Size(T* t) : __VSLArray<T, element_type, vsl_array_size<T>()>(t) {
			if constexpr (has_element_index<T>) {
				height = (*this)[T::size_index];
				weight = (*this)[T::size_index + 1];
			}
			else {
				height = (*this)[0];
				weight = (*this)[1];
			}
		}
	};

	template <typename ElementType>
	struct __VSLD2RectangleDefault {
		using element_type = ElementType;
		ElementType x, y, height, weight;
	};

	template<typename T>
	struct __VSLD2Rectangle {};

	template<typename T>
	concept is_rectangle = is_point<T> && is_size<T>;

	template<typename T>
		requires is_rectangle<T>
	struct __VSLD2Rectangle<T> : __VSLD2Point<T>, __VSLD2Size<T> {

		__VSLD2Rectangle(T* t) : __VSLD2Point<T>(t), __VSLD2Size<T>(t) {}
	};

	template <typename T>
		requires convert_to_vsl_at_least_array<T, 4>
	struct __VSLD2Rectangle<T> : __VSLArray<T, typename T::element_type, vsl_array_size<T>()> {
		using element_type = T::element_type;
		element_type& x, & y, & height, & weight;

		__VSLD2Rectangle(T* t) : __VSLArray<T, element_type, vsl_array_size<T>()>(t) {
			if constexpr (has_element_index<T>) {
				x = (*this)[T::element_index];
				y = (*this)[T::element_index + 1];
			}
			else {
				x = (*this)[0];
				y = (*this)[1];
			}
			if constexpr (has_element_index<T>) {
				height = (*this)[T::size_index];
				weight = (*this)[T::size_index + 1];
			}
			else {
				height = (*this)[2];
				weight = (*this)[3];
			}
		}
	};

	template<typename ElementType>
	struct __VSLDRGBColorDefault {
		using element_type = ElementType;
		element_type r, g, b;
	};

	template<typename ElementType>
	struct __VSLDRGBColorAccessor {
		using element_type = ElementType;
		__VSLDRGBColorAccessor(element_type& r, element_type& g, element_type& b)
			: r(r), g(g), b(b) {}

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
		requires is_size<T>
	struct __VSLDRGBColor<T> : public __VSLDRGBColorAccessor<typename T::element_type> {
		using element_type = T::element_type;

		__VSLDRGBColor(T* t)
			: __VSLDRGBColorAccessor<typename T::element_type>(t->r, t->g, t->b) {}
	};

	template <typename T>
		requires convert_to_vsl_at_least_array<T, 3>
	struct __VSLDRGBColor<T>
		: public __VSLDRGBColorAccessor<typename T::element_type>
		, public __VSLArray<T, typename T::element_type, vsl_array_size<T>()> {

		using element_type = T::element_type;

		__VSLDRGBColor(T* t) : __VSLDRGBColorAccessor<typename T::element_type>((*this)[0], (*this)[1], (*this)[2]), __VSLArray<T, element_type, vsl_array_size<T>()>(t) {
			if constexpr (has_element_index<T>) {
				this->r = (*this)[T::element_index];
				this->g = (*this)[T::element_index + 1];
				this->b = (*this)[T::element_index + 2];
			}
		}
	};
}