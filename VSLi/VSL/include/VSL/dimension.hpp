#pragma once
#include "define.hpp"

#include <concepts>

namespace VSL_NAMESPACE {
	struct D1 {
		constexpr static char dimension = 1;
	};

	struct D2 {
		constexpr static char dimension = 2;
	};

	struct D3 {
		constexpr static char dimension = 3;
	};

	template<int d>
	struct dimension {};

	template<>
	struct dimension<1> : D1 {
		using value = VSL_NAMESPACE::D1;
	};


	template<>
	struct dimension<2> : D2 {
		using value = VSL_NAMESPACE::D2;
	};


	template<>
	struct dimension<3> : D3 {
		using value = VSL_NAMESPACE::D3;
	};


	template<typename D>
	concept __has_char_dimension = requires{
		{D::dimension} -> std::convertible_to<char>;
	};


	template<typename D>
	concept is_dimension = __has_char_dimension<D> &&
						   std::same_as<D, VSL_NAMESPACE::D1> ||
						   std::same_as<D, VSL_NAMESPACE::D2> ||
					   	   std::same_as<D, VSL_NAMESPACE::D3>;
}