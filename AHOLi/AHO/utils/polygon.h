#pragma once
#include "../define.h"

#include "../core/Point.h"
#include "../core/ov.h"
#include <ranges>
#include <concepts>

namespace AHO_NAMESPACE {
	namespace concepts {
		template<typename T>
		concept _have_elment_dimention = requires {
			typename T::dimention;
			typename T::element_type;
		};

		template<typename T>
		concept is_line = requires(T t) {
            { _have_elment_dimention<T> };
            std::convertible_to<Line<typename std::remove_cvref_t<T>::element_type, typename std::remove_cvref_t<T>::dimention, typename std::remove_cvref_t<T>::coordinate_info>,
                                std::remove_cvref_t<T>>;
		};

		template<typename T>
		concept is_triangle = requires(T t) {
            { _have_elment_dimention<T> };
            std::convertible_to<Triangle<typename std::remove_cvref_t<T>::element_type, typename std::remove_cvref_t<T>::dimention, typename std::remove_cvref_t<T>::coordinate_info>,
                                std::remove_cvref_t<T>>;
		};

		template<typename T>
		concept is_points_object = requires (T t) {
			typename T::element_type;
			typename T::dimention;
            typename T::coordinate_info;
            { VSL_NAMESPACE::is_dimention<typename T::dimention> };
			{t.points()} -> std::ranges::range;
			{*std::begin(t.points())} -> is_point;
			std::views::common(t.points());
		};

		template<typename T>
		concept is_lines_object = requires (T t) {
			typename T::element_type;
			typename T::dimention;
            typename T::coordinate_info;
            { VSL_NAMESPACE::is_dimention<typename T::dimention> };
			{t.lines()} -> std::ranges::range;
			{*std::begin(t.lines())} -> is_line;
			std::views::common(t.lines());
		};

		template<typename T>
		concept is_triangles_object = requires (T t) {
			typename T::element_type;
			typename T::dimention;
            typename T::coordinate_info;
            { VSL_NAMESPACE::is_dimention<typename T::dimention> };
			{t.triangles()} -> std::ranges::range;
			{*std::begin(t.triangles())} -> is_triangle;
			std::views::common(t.triangles());
		};

		template<typename T>
		concept points_view_able = is_points_object<T> || is_point<T>;

        template<typename T>
        concept lines_view_able = is_lines_object<T> || is_line<T>;

        template<typename T>
        concept triangles_view_able = is_triangles_object<T> || is_triangle<T>;
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
		    requires concepts::points_view_able<std::remove_reference_t<T>>
		auto points(T&& t) {
			if constexpr (concepts::is_point<std::remove_reference_t<T>>) {
				return std::views::single(t);
			}
			else if constexpr (concepts::is_points_object<std::remove_reference_t<T>>) {
				return cast<_Point<typename std::remove_reference_t<T>::element_type, typename std::remove_reference_t<T>::dimention>>(t.points());
			}
		}

        template<typename T>
            requires concepts::lines_view_able<std::remove_reference_t<T>>
        auto lines(T&& t) {
            using PureT = std::remove_reference_t<T>;
            if constexpr (concepts::is_line<PureT>) {
                return std::views::single(t);
            }
            else if constexpr (concepts::is_lines_object<PureT>) {
                return cast<Line<typename PureT::element_type,
                        typename PureT::dimention,
                        typename PureT::coordinate_info>>(t.points());
            }
        }

        template<typename T>
        requires concepts::triangles_view_able<std::remove_reference_t<T>>
        auto triangles(T&& t) {
            using PureT = std::remove_reference_t<T>;
            if constexpr (concepts::is_triangle<PureT>) {
                return std::views::single(t);
            }
            else if constexpr (concepts::is_triangles_object<PureT>) {
                return cast<Triangle<typename PureT::element_type,
                        typename PureT::dimention,
                        typename PureT::coordinate_info>>(t.points());
            }
        }
	}
}