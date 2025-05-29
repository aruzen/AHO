#pragma once
#include "../define.h"

#include "../core/Point.h"
#include "../core/ov.h"
#include <ranges>

namespace AHO_NAMESPACE {
	namespace concepts {
		template<typename T>
		concept __have_elment_dimention = requires {
			typename T::dimention;
			typename T::element_type;
		};

		template<typename T>
		concept is_line = requires(T t) {
			__have_elment_dimention<T>;
			(Line<typename T::element_type, typename T::dimention>)t;
		};

		template<typename T>
		concept is_triangle = requires(T t) {
			__have_elment_dimention<T>;
			(Triangle<typename T::element_type, typename T::dimention>)t;
		};

		template<typename T>
		concept is_points_object = requires (T t) {
			typename T::element_type;
			typename T::dimention;
			VSL_NAMESPACE::is_dimention<typename T::dimention>;
			{t.points()} -> std::ranges::range;
			{*t.points().begin()} -> is_point;
			std::views::common(t.points());
		};

		template<typename T>
		concept is_lines_object = requires (T t) {
			typename T::element_type;
			typename T::dimention;
			VSL_NAMESPACE::is_dimention<typename T::dimention>;
			{t.lines()} -> std::ranges::range;
			{*t.lines().begin()} -> is_line;
			std::views::common(t.lines());
		};

		template<typename T>
		concept is_triangles_object = requires (T t) {
			typename T::element_type;
			typename T::dimention;
			VSL_NAMESPACE::is_dimention<typename T::dimention>;
			{t.triangles()} -> std::ranges::range;
			{*t.triangles().begin()} -> is_triangle;
			std::views::common(t.triangles());
		};

		template<typename T>
		concept points_view_able = is_points_object<T> || is_point<T>;
	}

	namespace views {
		template<typename InnerView>
		struct wrap_view : std::ranges::view_interface<wrap_view<InnerView>> {
			using inner_view_t = InnerView;
			using iterator = decltype(std::declval<InnerView>().begin());

			inner_view_t view;

			wrap_view(inner_view_t f) : view(f) {};

			iterator begin() {
				return view.begin();
			}

			iterator end() {
				return view.end();
			}
		};

		template<typename To>
		struct _Castor {
			constexpr To operator ()(auto a) const {
				return (To)a;
			}
		};

		template<typename To, typename F>
		auto cast(F&& f) {
			std::vector<To> result(f.size());
			auto view = std::ranges::transform_view(std::views::common(std::forward<F>(f)), _Castor<To>());
			int idx = 0;
			for (auto e : view) {
				result[idx] = e;
				idx++;
			};
			return result;
		}

		template<typename T>
		auto points(T&& t) {
			if constexpr (AHO_NAMESPACE::concepts::is_point<std::remove_reference_t<T>>) {
				return std::views::single(std::declval<T>());
			}
			else if constexpr (concepts::is_points_object<std::remove_reference_t<T>>) {
				return cast<_Point<typename std::remove_reference_t<T>::element_type, typename std::remove_reference_t<T>::dimention>>(t.points());
			}
		}
	}
}