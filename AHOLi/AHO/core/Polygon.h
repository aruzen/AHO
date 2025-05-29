#pragma once
#include "../define.h"

#include <VSL/dimention.h>

#include "Vector.h"
#include "Point.h"
#include "Triangle.h"

#include "../utils/polygon.h"

#include <map>
#include <set>
#include <initializer_list>
#include <boost/bimap.hpp>
#include <numeric>

namespace AHO_NAMESPACE {
	// TODO
	template<typename R,
			 VSL_NAMESPACE::is_dimention D,
			 typename CoordinateInfo,
			 typename Size,
			 typename Alloc>
	struct Polygon : public std::vector<IndexedTriangle<Size>, Alloc> {
        using dimention = D;
        using element_type = R;
        using coordinate_info = CoordinateInfo;

		boost::bimap<AHO_NAMESPACE::_Point<R, D, CoordinateInfo>, Size> _points;

		constexpr Polygon() : std::vector<IndexedTriangle<Size>, Alloc>() {};
		constexpr Polygon(std::initializer_list<AHO_NAMESPACE::Triangle<R, D, CoordinateInfo>> init);

		template<typename T>
            requires AHO_NAMESPACE::concepts::points_view_able<T> || AHO_NAMESPACE::concepts::triangles_view_able<T>
		constexpr Polygon<R, D, CoordinateInfo, Size, Alloc> operator +(T t) const;

        template<typename T>
            requires AHO_NAMESPACE::concepts::points_view_able<T> || AHO_NAMESPACE::concepts::triangles_view_able<T>
        constexpr Polygon<R, D, CoordinateInfo, Size, Alloc>& operator +=(T t);

		constexpr double area();

		constexpr auto points();

        // 重めの処理
        constexpr auto lines();

        constexpr auto triangles();
	};

    // ===========================================================================================

    template<typename R, VSL_NAMESPACE::is_dimention D, typename CI, typename Size, typename Alloc>
    inline constexpr AHO_NAMESPACE::Polygon<R, D, CI, Size, Alloc>::Polygon(std::initializer_list<AHO_NAMESPACE::Triangle<R, D, CI>> init)
            : std::vector<IndexedTriangle<Size>, Alloc>() {
        for (auto i : init) {
            (*this) += i;
        }
    }

    template<typename R, VSL_NAMESPACE::is_dimention D, typename CI, typename Size, typename Alloc>
    template<typename T>
        requires AHO_NAMESPACE::concepts::points_view_able<T> || AHO_NAMESPACE::concepts::triangles_view_able<T>
    inline constexpr Polygon<R, D, CI, Size, Alloc> AHO_NAMESPACE::Polygon<R, D, CI, Size, Alloc>::operator+(T t) const
	{
        auto copy = Polygon<R, D, CI, Size, Alloc>(*this);
        copy += t;
        return copy;
	}

    template<typename R, vsl::is_dimention D, typename CI, typename Size, typename Alloc>
    template<typename T>
    requires AHO_NAMESPACE::concepts::points_view_able<T> || AHO_NAMESPACE::concepts::triangles_view_able<T>
    constexpr Polygon<R, D, CI, Size, Alloc>& Polygon<R, D, CI, Size, Alloc>::operator+=(T t) {
        if constexpr (AHO_NAMESPACE::concepts::triangles_view_able<T>) {
            for (Triangle<R, D, CI> triangle : views::triangles(t)) {
                // 新しいのを見つけた場合
                IndexedTriangle<Size> indexed_tri;
                size_t i = 0;
                for (const auto& point : t.points()) {
                    // TODO ランダムアクセス可能でない場合アクセスできないのであとで直す
                    auto point_itr = this->_points.left.find(point);
                    if (point_itr == this->_points.left.end()) {
                        indexed_tri[i] = Index<Size>(this->_points.size());
                        using vt = typename decltype(_points)::value_type;
                        this->_points.insert(vt(point, this->_points.size()));
                    } else {
                        indexed_tri[i] = Index<Size>(point_itr->second);
                    }
                    i++;
                }
                this->push_back(indexed_tri);
            }
        } else if constexpr (AHO_NAMESPACE::concepts::points_view_able<T>) {
            // TODO
        }
        return *this;
    }

	template<typename R, VSL_NAMESPACE::is_dimention D, typename CI, typename Size, typename Alloc>
	inline constexpr auto AHO_NAMESPACE::Polygon<R, D, CI, Size, Alloc>::points()
	{
		return _points.left | std::views::transform([](const auto& pair) {
            return pair.first;
        });
	}

    template<typename R, VSL_NAMESPACE::is_dimention D, typename CI, typename Size, typename Alloc>
    inline constexpr auto AHO_NAMESPACE::Polygon<R, D, CI, Size, Alloc>::lines()
    {
        auto max = (3*this->size());
        return std::views::iota((decltype(max))0, max) |
        std::views::transform([this](decltype(max) i){
            IndexedTriangle<Size> t = (*this)[i/3];
            return Line<R, D, CI>(_points.right.at(t[i%3].index), _points.right.at(t[(i+1)%3].index));
        });
    }

    template<typename R, VSL_NAMESPACE::is_dimention D, typename CI, typename Size, typename Alloc>
    inline constexpr auto AHO_NAMESPACE::Polygon<R, D, CI, Size, Alloc>::triangles()
    {
        return *this | std::views::transform([this](IndexedTriangle<Size> t) {
            return Triangle<R, D, CI>(_points.right.at(t.idx1.index),
                                     _points.right.at(t.idx2.index),
                                     _points.right.at(t.idx3.index));
        });
    }

    template<typename R, VSL_NAMESPACE::is_dimention D, typename CI, typename Size, typename Alloc>
    inline constexpr double AHO_NAMESPACE::Polygon<R, D, CI, Size, Alloc>::area()
    {
        auto triangles = this->triangles();
        return std::accumulate(triangles.begin(), triangles.end(), 0.0);
    }

    static_assert(concepts::is_triangles_object<Polygon<double, VSL_NAMESPACE::D2>>);
    static_assert(concepts::is_lines_object<Polygon<double, VSL_NAMESPACE::D2>>);
    static_assert(concepts::is_points_object<Polygon<double, VSL_NAMESPACE::D2>>);
}