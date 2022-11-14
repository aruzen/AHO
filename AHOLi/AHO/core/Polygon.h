#pragma once
#include "../define.h"

#include "../../VSL/dimention.h"

#include "Vector.h"
#include "Point.h"

#include "../utils/polygon.h"

#include <map>
#include <initializer_list>

namespace AHO_NAMESPACE {
	template<typename R = int, VSL_NAMESPACE::is_dimention D = VSL_NAMESPACE::VSL_DEFAULT_DIMENTION_STRUCT, typename Size = size_t, typename Alloc = std::vector<IndexedTriangle<Size>>::Allocator>
	struct Polygon : public std::vector<Alloc, IndexedTriangle<Size>> {
		std::map<AHO_NAMESPACE::_Point<R, D>, Size> points;

		constexpr Polygon() {}; 
		constexpr Polygon(std::initializer_list<AHO_NAMESPACE::Triangle<R, D>> init) {};

		template<AHO_NAMESPACE::points_view_able T>
		constexpr Polygon<R, D> operator +(T t);

		constexpr double area();

		constexpr std::array<AHO_NAMESPACE::_Point<R, D>, 3> points();
	};
}