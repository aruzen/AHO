#pragma once
#include "../point.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <concepts>
#include <stdexcept>

namespace AHO_NAMESPACE::math {
    namespace detail {
        template<typename T>
        concept coordinate_value = requires(const T& value) {
            typename T::ElementType;
            typename T::CoordinateInfo;
            requires VSL_NAMESPACE::is_dimension<typename T::Dimension>;
            requires std::floating_point<typename T::ElementType>;
            value.value.___AN1;
        };
    } // namespace detail

    template<std::floating_point T>
    constexpr T lerp(T first, T second, T amount);

    template<detail::coordinate_value T>
    constexpr T lerp(const T& first, const T& second, typename T::ElementType amount);

    template<std::floating_point T, std::size_t size>
    constexpr std::array<T, size> lerp(const std::array<T, size>& first, const std::array<T, size>& second, T amount);

    // 有限で昇順のedgeを指定する。範囲外は0/1へclampし、NaNは拒否する。
    template<std::floating_point T>
    constexpr T smoothstep(T first_edge, T second_edge, T value);

    template<detail::coordinate_value T>
    auto distance(const T& first, const T& second);

    // 既存のnormalize()とは別に、ゼロ長・非有限の入力を拒否する。
    template<concepts::is_vector T>
    requires std::floating_point<typename T::ElementType>
    auto normalized(const T& value);

    // 頂点の転送時に座標ラッパーの配置へ依存しない。
    template<detail::coordinate_value T>
    constexpr auto to_array(const T& value);

    template<std::floating_point T>
    constexpr T lerp(T first, T second, T amount) {
        return std::lerp(first, second, amount);
    }

    template<detail::coordinate_value T>
    constexpr T lerp(const T& first, const T& second, typename T::ElementType amount) {
        const auto a = to_array(first), b = to_array(second);
        if constexpr (T::Dimension::DIMENSION == 1)
            return T{math::lerp(a[0], b[0], amount)};
        else if constexpr (T::Dimension::DIMENSION == 2)
            return T{math::lerp(a[0], b[0], amount), math::lerp(a[1], b[1], amount)};
        else
            return T{math::lerp(a[0], b[0], amount), math::lerp(a[1], b[1], amount), math::lerp(a[2], b[2], amount)};
    }

    template<std::floating_point T, std::size_t size>
    constexpr std::array<T, size> lerp(const std::array<T, size>& first, const std::array<T, size>& second, T amount) {
        std::array<T, size> result{};
        for (std::size_t i = 0; i < size; ++i)
            result[i] = math::lerp(first[i], second[i], amount);
        return result;
    }

    template<std::floating_point T>
    constexpr T smoothstep(T first_edge, T second_edge, T value) {
        if (!std::isfinite(first_edge) || !std::isfinite(second_edge) || !(first_edge < second_edge) || std::isnan(value))
            throw std::invalid_argument("smoothstep requires finite ascending edges and a non-NaN value");
        if (value <= first_edge)
            return T{0};
        if (value >= second_edge)
            return T{1};
        // edgeの差が型の最大値を超える場合も、中間の補間率を保つ。
        const auto width = second_edge - first_edge;
        const auto t = std::isfinite(width) ? (value - first_edge) / width
                                            : (value / T{2} - first_edge / T{2}) / (second_edge / T{2} - first_edge / T{2});
        return t * t * (T{3} - T{2} * t);
    }

    template<detail::coordinate_value T>
    auto distance(const T& first, const T& second) {
        const auto a = to_array(first), b = to_array(second);
        if constexpr (T::Dimension::DIMENSION == 1)
            return std::abs(b[0] - a[0]);
        else if constexpr (T::Dimension::DIMENSION == 2)
            return std::hypot(b[0] - a[0], b[1] - a[1]);
        else
            return std::hypot(b[0] - a[0], b[1] - a[1], b[2] - a[2]);
    }

    template<concepts::is_vector T>
    requires std::floating_point<typename T::ElementType>
    auto normalized(const T& value) {
        const auto length = value.length();
        if (!std::isfinite(length) || length <= 0)
            throw std::invalid_argument("Cannot normalize a zero or non-finite vector");
        return value / length;
    }

    template<detail::coordinate_value T>
    constexpr auto to_array(const T& value) {
        using R = typename T::ElementType;
        if constexpr (T::Dimension::DIMENSION == 1)
            return std::array<R, 1>{value.value.___AN1};
        else if constexpr (T::Dimension::DIMENSION == 2)
            return std::array<R, 2>{value.value.___AN1, value.value.___AN2};
        else
            return std::array<R, 3>{value.value.___AN1, value.value.___AN2, value.value.___AN3};
    }
} // namespace AHO_NAMESPACE::math
