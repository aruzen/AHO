#pragma once
#include "define.hpp"

#include <type_traits>
#include <concepts>
#include <optional>

namespace AHO_NAMESPACE::concepts {
    namespace helper {
        template<class T>
        struct is_std_tuple : std::false_type {};

        template<class... Ts>
        struct is_std_tuple<std::tuple<Ts...>> : std::true_type {};

        template<size_t Index, typename... Args>
        struct GetType {};

        template<size_t Index, typename T, typename... Args>
            requires (Index == 0)
        struct GetType<Index, T, Args...> {
            using value = T;
        };

        template<size_t Index, typename T, typename... Args>
            requires (Index != 0)
        struct GetType<Index, T, Args...> {
            using value = GetType<Index-1, Args...>;
        };

        template<size_t Index, typename T, typename... Args>
        using get_type_v = GetType<Index, T, Args...>::value;
    }

    template<class T>
    concept is_std_tuple = helper::is_std_tuple<std::remove_cvref_t<T>>::value;

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
		t* r;
	};

	template<typename T, typename R>
	concept division_as = requires(T t, R r) {
		t / r;
	};

	template<typename T, typename R>
	concept cast_as = requires(T t) {
		(R)t;
	};

	template<typename T>
	consteval bool __sames_as() {
		return true;
	}

	template<typename T, typename R, typename... Args>
	consteval bool __sames_as() {
		if constexpr (std::same_as<T, R>) {
			return __sames_as<T, Args...>();
		}
		return false;
	}

	template<typename T, typename... Args>
	concept sames_as = (0 == sizeof...(Args) && true) ||
		(1 == sizeof...(Args) && std::same_as<T, typename helper::GetType<0, Args...>::value>) ||
		(2 <= sizeof...(Args) && __sames_as<T, Args...>());

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