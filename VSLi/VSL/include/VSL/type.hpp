#pragma once
#include "define.hpp"

#include <concepts>
#include <compare>

namespace VSL_NAMESPACE {
    template<typename C, typename T = char>
    class type {
        const char* name_;
        const T index_;

    public:
        consteval type(const char* name_, T index_);

        constexpr const char* name() const;
        constexpr const T index() const;

        constexpr auto operator<=>(const C& t) const;
    };

    template<typename C, typename T>
    inline consteval type<C, T>::type(const char* name_, T index_) : name_(name_), index_(index_) {}

    template<typename C, typename T>
    inline constexpr const char* type<C, T>::name() const {
        return name_;
    }

    template<typename C, typename T>
    inline constexpr const T type<C, T>::index() const {
        return index_;
    }

    template<typename C, typename T>
    inline constexpr auto type<C, T>::operator<=>(const C& t) const {
        return t.index_ <=> index_;
    }
} // namespace VSL_NAMESPACE
