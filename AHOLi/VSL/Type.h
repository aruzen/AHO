#pragma once
#include "define.h"

#include <concepts>

namespace VSL_NAMESPACE {
	template<typename C, typename T = char>
	class Type {
		const char* name_;
		const T index_;
	public:
		consteval Type(const char* name_, T index_);

		constexpr const char* name();
		constexpr const T index();

		constexpr bool operator <=>(const C& t);
	};

	template<typename C, typename T>
	inline consteval Type<C, T>::Type(const char* name_, T index_) : name_(name_), index_(index_) {}

	template<typename C, typename T>
	inline constexpr const char* Type<C, T>::name()
	{
		return name_;
	}

	template<typename C, typename T>
	inline constexpr const T Type<C, T>::index()
	{
		return index_;
	}

	template<typename C, typename T>
	inline constexpr bool Type<C, T>::operator<=>(const C& t)
	{
		return t.index_ <=> index_;
	}
}