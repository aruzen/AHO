#pragma once
#include "define.hpp"

#include <type_traits>
#include <concepts>
#include <optional>

namespace AHO_NAMESPACE::concepts {
    namespace helper {
        template<class T>
        struct IsStdTuple : std::false_type {};

        template<class... Ts>
        struct IsStdTuple<std::tuple<Ts...>> : std::true_type {};

        template<size_t index, typename... Args>
        struct GetType {};

        template<size_t index, typename T, typename... Args>
        requires(index == 0)
        struct GetType<index, T, Args...> {
            using Value = T;
        };

        template<size_t index, typename T, typename... Args>
        requires(index != 0)
        struct GetType<index, T, Args...> {
            using Value = typename GetType<index - 1, Args...>::Value;
        };

        template<size_t index, typename T, typename... Args>
        using GetTypeV = GetType<index, T, Args...>::Value;
    } // namespace helper

    template<class T>
    concept is_std_tuple = helper::IsStdTuple<std::remove_cvref_t<T>>::value;

    template<typename T>
    concept plusable = requires(T t) {
        +t;
    };

    template<typename T>
    concept minusable = requires(T t) {
        -t;
    };

    template<typename T, typename R>
    concept add_as = requires(T t, R r) {
        t + r;
    };

    template<typename T, typename R>
    concept subtract_as = requires(T t, R r) {
        t - r;
    };

    template<typename T, typename R>
    concept multiply_as = requires(T t, R r) {
        t * r;
    };

    template<typename T, typename R>
    concept division_as = requires(T t, R r) {
        t / r;
    };

    template<typename T, typename R>
    concept add_assign_as = requires(T t, R r) {
        t += r;
    };

    template<typename T, typename R>
    concept subtract_assign_as = requires(T t, R r) {
        t -= r;
    };

    template<typename T, typename R>
    concept multiply_assign_as = requires(T t, R r) {
        t *= r;
    };

    template<typename T, typename R>
    concept division_assign_as = requires(T t, R r) {
        t /= r;
    };

    template<typename T, typename R>
    concept cast_as = requires(T t) {
        (R) t;
    };

    template<typename T>
    consteval bool sames_as_() {
        return true;
    }

    template<typename T, typename R, typename... Args>
    consteval bool sames_as_() {
        if constexpr (std::same_as<T, R>) {
            return sames_as_<T, Args...>();
        }
        return false;
    }

    template<typename T, typename... Args>
    concept sames_as = (0 == sizeof...(Args) && true) ||
                       (1 == sizeof...(Args) && std::same_as<T, typename helper::GetType<0, Args...>::Value>) ||
                       (2 <= sizeof...(Args) && sames_as_<T, Args...>());

    template<typename T>
    concept is_standard_object = requires(T t) {
        { t.area() } -> std::convertible_to<double>;
    };

    template<typename T, typename R>
    concept is_intersect_object = requires(T t, R r) {
        { t.intersect(r) } -> std::convertible_to<bool>;
        { t.contains(r) } -> std::convertible_to<bool>;
    };
} // namespace AHO_NAMESPACE::concepts
