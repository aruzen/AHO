#pragma once
#include "../define.hpp"

#include "../core/point.hpp"
#include "../core/ov.hpp"
#include <ranges>
#include <concepts>
#include <utility>
#include <vector>

namespace AHO_NAMESPACE {
    namespace concepts {
        template<typename T>
        concept _have_elment_dimension = requires {
            typename T::Dimension;
            typename T::ElementType;
        };

        template<typename T>
        concept is_line = requires(T t) {
            requires _have_elment_dimension<std::remove_cvref_t<T>>;
            requires std::convertible_to<std::remove_cvref_t<T>,
                                         Line<typename std::remove_cvref_t<T>::ElementType, typename std::remove_cvref_t<T>::Dimension, typename std::remove_cvref_t<T>::CoordinateInfo>>;
        };

        template<typename T>
        concept is_triangle = requires(T t) {
            requires _have_elment_dimension<std::remove_cvref_t<T>>;
            requires std::convertible_to<std::remove_cvref_t<T>,
                                         Triangle<typename std::remove_cvref_t<T>::ElementType, typename std::remove_cvref_t<T>::Dimension, typename std::remove_cvref_t<T>::CoordinateInfo>>;
        };

        template<typename T>
        concept is_points_object = requires(T t) {
            typename T::ElementType;
            typename T::Dimension;
            typename T::CoordinateInfo;
            requires VSL_NAMESPACE::is_dimension<typename T::Dimension>;
            { t.points() } -> std::ranges::range;
            { *std::begin(t.points()) } -> is_point;
            std::views::common(t.points());
        };

        template<typename T>
        concept is_lines_object = requires(T t) {
            typename T::ElementType;
            typename T::Dimension;
            typename T::CoordinateInfo;
            requires VSL_NAMESPACE::is_dimension<typename T::Dimension>;
            { t.lines() } -> std::ranges::range;
            { *std::begin(t.lines()) } -> is_line;
            std::views::common(t.lines());
        };

        template<typename T>
        concept is_triangles_object = requires(T t) {
            typename T::ElementType;
            typename T::Dimension;
            typename T::CoordinateInfo;
            requires VSL_NAMESPACE::is_dimension<typename T::Dimension>;
            { t.triangles() } -> std::ranges::range;
            { *std::begin(t.triangles()) } -> is_triangle;
            std::views::common(t.triangles());
        };

        template<typename T>
        concept points_view_able = is_points_object<T> || is_point<T>;

        template<typename T>
        concept lines_view_able = is_lines_object<T> || is_line<T>;

        template<typename T>
        concept triangles_view_able = is_triangles_object<T> || is_triangle<T>;
    } // namespace concepts

    namespace views {
        template<typename InnerView>
        struct WrapView : std::ranges::view_interface<WrapView<InnerView>> {
            using InnerViewT = InnerView;
            using Iterator = std::ranges::iterator_t<InnerView>;
            using Sentinel = std::ranges::sentinel_t<InnerView>;

            InnerViewT view;

            WrapView(InnerViewT f) : view(std::move(f)) {};

            Iterator begin() {
                return view.begin();
            }

            Sentinel end() {
                return view.end();
            }
        };

        template<typename To>
        struct Castor {
            constexpr To operator()(auto a) const {
                return (To)a;
            }
        };

        template<typename To, typename F>
        auto cast(F&& f) {
            std::vector<To> result;
            if constexpr (std::ranges::sized_range<F>)
                result.reserve(std::ranges::size(f));
            for (auto&& element : f)
                result.push_back(Castor<To>{}(element));
            return result;
        }

        template<typename T>
        requires concepts::points_view_able<std::remove_reference_t<T>>
        auto points(T&& t) {
            if constexpr (concepts::is_point<std::remove_reference_t<T>>) {
                return std::views::single(t);
            } else if constexpr (concepts::is_points_object<std::remove_reference_t<T>>) {
                return cast<Point_<typename std::remove_reference_t<T>::ElementType,
                                   typename std::remove_reference_t<T>::Dimension,
                                   typename std::remove_reference_t<T>::CoordinateInfo>>(t.points());
            }
        }

        template<typename T>
        requires concepts::lines_view_able<std::remove_reference_t<T>>
        auto lines(T&& t) {
            using PureT = std::remove_reference_t<T>;
            if constexpr (concepts::is_line<PureT>) {
                return std::views::single(t);
            } else if constexpr (concepts::is_lines_object<PureT>) {
                return cast<Line<typename PureT::ElementType,
                                 typename PureT::Dimension,
                                 typename PureT::CoordinateInfo>>(t.lines());
            }
        }

        template<typename T>
        requires concepts::triangles_view_able<std::remove_reference_t<T>>
        auto triangles(T&& t) {
            using PureT = std::remove_reference_t<T>;
            if constexpr (concepts::is_triangle<PureT>) {
                return std::views::single(t);
            } else if constexpr (concepts::is_triangles_object<PureT>) {
                return cast<Triangle<typename PureT::ElementType,
                                     typename PureT::Dimension,
                                     typename PureT::CoordinateInfo>>(t.triangles());
            }
        }
    } // namespace views
} // namespace AHO_NAMESPACE
