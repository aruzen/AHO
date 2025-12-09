#pragma once
#include "define.hpp"

#include <concepts>

namespace VSL_NAMESPACE {
	template<typename C, typename T = char>
	class type {
		const char* name_;
		const T index_;
	public:
		consteval type(const char* name_, T index_);

		constexpr const char* name();
		constexpr const T index();

		constexpr bool operator <=>(const C& t);
	};

	template<typename C, typename T>
	inline consteval type<C, T>::type(const char* name_, T index_) : name_(name_), index_(index_) {}

	template<typename C, typename T>
	inline constexpr const char* type<C, T>::name()
	{
		return name_;
	}

	template<typename C, typename T>
	inline constexpr const T type<C, T>::index()
	{
		return index_;
	}

	template<typename C, typename T>
	inline constexpr bool type<C, T>::operator<=>(const C& t)
	{
		return t.index_ <=> index_;
	}
}