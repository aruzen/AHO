#pragma once
#include "define.hpp"

#include <concepts>
#include <utility>

namespace VSL_NAMESPACE::concepts {
	template<typename Parent, typename Target, typename T>
	concept searcher = requires(T t, Parent s) {
		{t.search(s)} -> std::same_as<Target>;
		T();
	};

	template<typename Target, typename T>
	concept checker = requires(T t,Target target) {
		{t.check(target)} -> std::same_as<bool>;
		T();
	};

	template<typename Material, typename Target, typename T>
	concept creator = requires(T t, Material material) {
		{t.create(material)} -> std::same_as<Target>;
		T();
	};

	template<typename T, typename ...Args>
	concept initializer = requires(Args ...args) {
		T(std::forward<Args>(args)...);
	};
}