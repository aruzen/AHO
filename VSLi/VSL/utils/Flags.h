#pragma once
#include "../define.h"

#include <concepts>

namespace VSL_NAMESPACE {
#define ___VSL_EXPAND_FLAGS_OPERATOR(T) \
	constexpr T operator|(T l, T r) { \
		return static_cast<T>(static_cast<std::underlying_type_t<T>>(l) | static_cast<std::underlying_type_t<T>>(r)); \
	} \
 \
	constexpr T& operator|=(T& l, T r) { \
		l = static_cast<T>(static_cast<std::underlying_type_t<T>>(l) | static_cast<std::underlying_type_t<T>>(r)); \
		return l; \
	} \
 \
	constexpr T operator&(T l, T r) { \
		return static_cast<T>(static_cast<std::underlying_type_t<T>>(l) & static_cast<std::underlying_type_t<T>>(r)); \
	} \
 \
	constexpr T& operator&=(T& l, T r) { \
		l = static_cast<T>(static_cast<std::underlying_type_t<T>>(l) & static_cast<std::underlying_type_t<T>>(r)); \
		return l; \
	} \
 \
	constexpr T operator^(T l, T r) { \
		return static_cast<T>(static_cast<std::underlying_type_t<T>>(l) ^ static_cast<std::underlying_type_t<T>>(r)); \
	} \
 \
	constexpr T& operator^=(T& l, T r) { \
		l = static_cast<T>(static_cast<std::underlying_type_t<T>>(l) ^ static_cast<std::underlying_type_t<T>>(r)); \
		return l; \
	}
}