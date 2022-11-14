#pragma once
#include "define.h"

#include <concepts>

namespace AHO_NAMESPACE {
	template<typename T, typename R>
	concept add_as = requires(T t, R r) {
		t + r;
	};

	template<typename T, typename R>
	concept subtract_as = requires(T t, R r) {
		t - r;
	};

	template<typename T, typename R>
	concept multily_as = requires(T t, R r) {
		t * r;
	};

	template<typename T, typename R>
	concept division_as = requires(T t, R r) {
		t / r;
	};

	template<typename T, typename R>
	concept cast_as = requires(T t) {
		(R)t;
	};

	template<typename T, typename R, typename... Args>
	concept __sames_as = std::same_as<T, R> && (0 == sizeof...(Args) || __sames_as<T, Args...>);

	template<typename T,typename... Args>
	concept sames_as =  (0 == sizeof...(Args) && true) ||
						(1 == sizeof...(Args) && std::same_as<T, Args...>) ||
						(2 <= sizeof...(Args) && __sames_as<T, Args...>);

	template<typename T>
	concept is_standard_object = requires(T t) {
		{t.area()} -> std::convertible_to<double>;
	};

	template<typename T, typename R>
	concept is_intersect_object = requires(T t, R r) {
		{ t.intersect(r) } -> std::convertible_to<bool>;
		{ t.cotains(r) } -> std::convertible_to<bool>;
	};
}