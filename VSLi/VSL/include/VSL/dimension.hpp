#pragma once
#include "define.hpp"

#include <concepts>

namespace VSL_NAMESPACE {
    struct D1 {
        constexpr static char DIMENSION = 1;
    };

    struct D2 {
        constexpr static char DIMENSION = 2;
    };

    struct D3 {
        constexpr static char DIMENSION = 3;
    };

    struct D4 {
        constexpr static char DIMENSION = 4;
    };

    template<int d>
    struct Dimension {};

    template<>
    struct Dimension<1> : D1 {
        using Value = VSL_NAMESPACE::D1;
    };

    template<>
    struct Dimension<2> : D2 {
        using Value = VSL_NAMESPACE::D2;
    };

    template<>
    struct Dimension<3> : D3 {
        using Value = VSL_NAMESPACE::D3;
    };

    template<>
    struct Dimension<4> : D4 {
        using Value = VSL_NAMESPACE::D4;
    };

    template<typename D>
    concept __has_char_dimension = requires {
        { D::DIMENSION } -> std::convertible_to<char>;
    };

    template<typename D>
    concept is_dimension = __has_char_dimension<D> &&
                               std::same_as<D, VSL_NAMESPACE::D1> ||
                           std::same_as<D, VSL_NAMESPACE::D2> ||
                           std::same_as<D, VSL_NAMESPACE::D3> ||
                           std::same_as<D, VSL_NAMESPACE::D4>;

    template<typename D>
    concept is_image_dimension = std::same_as<D, D1> || std::same_as<D, D2> || std::same_as<D, D3>;
} // namespace VSL_NAMESPACE
