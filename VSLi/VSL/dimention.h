#pragma once
#include "define.h"

#include <concepts>

namespace VSL_NAMESPACE {
	struct D1 {
		constexpr static char dimention = 1;
	};

	struct D2 {
		constexpr static char dimention = 2;
	};

	struct D3 {
		constexpr static char dimention = 3;
	};

	template<int d>
	struct Dimention {};

	template<>
	struct Dimention<1> : D1 {
		using value = VSL_NAMESPACE::D1;
	};


	template<>
	struct Dimention<2> : D2 {
		using value = VSL_NAMESPACE::D2;
	};


	template<>
	struct Dimention<3> : D3 {
		using value = VSL_NAMESPACE::D3;
	};


	template<typename D>
	concept __has_char_dimantion = requires{
		{D::dimention} -> std::convertible_to<char>;
	};


	template<typename D>
	concept is_dimention = __has_char_dimantion<D> &&
						   std::same_as<D, VSL_NAMESPACE::D1> ||
						   std::same_as<D, VSL_NAMESPACE::D2> ||
					   	   std::same_as<D, VSL_NAMESPACE::D3>;
}