#pragma once
#include "define.hpp"

#include <concepts>
#include <compare>

namespace VSL_NAMESPACE {
    template<typename C, typename T = char>
    class Type_ {
        const char* _name;
        const T _index;

    public:
        consteval Type_(const char* name, T index);

        constexpr const char* name() const;
        constexpr const T index() const;

        constexpr auto operator<=>(const C& t) const;
        friend constexpr auto operator<=>(const C& first, const C& second) {
            return second._index <=> first._index;
        }
    };

    template<typename C, typename T>
    inline consteval Type_<C, T>::Type_(const char* name, T index) : _name(name), _index(index) {}

    template<typename C, typename T>
    inline constexpr const char* Type_<C, T>::name() const {
        return _name;
    }

    template<typename C, typename T>
    inline constexpr const T Type_<C, T>::index() const {
        return _index;
    }

    template<typename C, typename T>
    inline constexpr auto Type_<C, T>::operator<=>(const C& t) const {
        return t._index <=> _index;
    }
} // namespace VSL_NAMESPACE
