#pragma once
#include "../define.h"

#include "../../VSL/dimention.h"

#include "Vector.h"
#include "Point.h"

#include "../utils/polygon.h"

#include <map>
#include <initializer_list>

namespace AHO_NAMESPACE {
	// TODO
	template<typename R = int,
			 VSL_NAMESPACE::is_dimention D,
			 typename CoordinateInfo,
			 typename Size,
			 typename Alloc>
	struct Polygon : public std::vector<Alloc, IndexedTriangle<Size>> {
		std::map<AHO_NAMESPACE::_Point<R, D, CoordinateInfo>, Size> points;

		constexpr Polygon() {}; 
		constexpr Polygon(std::initializer_list<AHO_NAMESPACE::Triangle<R, D, CI>> init) {};

		template<AHO_NAMESPACE::concepts::points_view_able T>
		constexpr Polygon<R, D, CoordinateInfo, Size, Alloc> operator +(T t);

		constexpr double area();

		constexpr std::array<AHO_NAMESPACE::_Point<R, D, CoordinateInfo>, 3> points();
	};

	template<typename R, VSL_NAMESPACE::is_dimention D, typename CI, typename Size, typename Alloc>
	template<AHO_NAMESPACE::concepts::points_view_able T>
	inline constexpr Polygon<R, D, CI, Size, Alloc> AHO_NAMESPACE::Polygon<R, D, CI, Size, Alloc>::operator+(T t)
	{
		Polygon<R, D, CI, Size, Alloc> copy(*this);

		return copy;
	}

	template<typename R, VSL_NAMESPACE::is_dimention D, typename CI, typename Size, typename Alloc>
	inline constexpr double AHO_NAMESPACE::Polygon<R, D, CI, Size, Alloc>::area()
	{
		return 0.0;
	}

	template<typename R, VSL_NAMESPACE::is_dimention D, typename CI, typename Size, typename Alloc>
	inline constexpr std::array<AHO_NAMESPACE::_Point<R, D, CI>, 3> AHO_NAMESPACE::Polygon<R, D, CI, Size, Alloc>::points()
	{
		return std::array<AHO_NAMESPACE::_Point<R, D, CI>, 3>();
	}
}