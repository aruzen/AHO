#pragma once
#include "../define.hpp"

#include "VSL/dimension.hpp"

#include "index.hpp"

#include <vector>
#include "math/coordinate.hpp"

namespace AHO_NAMESPACE {
    template<typename R = int,
            VSL_NAMESPACE::is_dimension D = VSL_NAMESPACE::VSL_DEFAULT_dimension_STRUCT,
            typename CoordinateInfo = AHO_NAMESPACE::coordinate::_DefaultCoordinateInfo<D>::value>
    struct _Point;

    template<typename R = int,
            VSL_NAMESPACE::is_dimension D = VSL_NAMESPACE::VSL_DEFAULT_dimension_STRUCT,
            typename CoordinateInfo = typename AHO_NAMESPACE::coordinate::_DefaultCoordinateInfo<D>::value>
    struct _Vector;

	template<typename R = int, VSL_NAMESPACE::is_dimension D = VSL_NAMESPACE::VSL_DEFAULT_dimension_STRUCT,
			 typename CoordinateInfo = AHO_NAMESPACE::coordinate::_DefaultCoordinateInfo<D>::value>
	struct Line;

	template<typename R = int, VSL_NAMESPACE::is_dimension D = VSL_NAMESPACE::VSL_DEFAULT_dimension_STRUCT,
			 typename CoordinateInfo = AHO_NAMESPACE::coordinate::_DefaultCoordinateInfo<D>::value>
	struct PtrLine;

	template<typename R = int, VSL_NAMESPACE::is_dimension D = VSL_NAMESPACE::VSL_DEFAULT_dimension_STRUCT,
			 typename CoordinateInfo = AHO_NAMESPACE::coordinate::_DefaultCoordinateInfo<D>::value>
	struct Triangle; // points_object

	template<typename R = int, VSL_NAMESPACE::is_dimension D = VSL_NAMESPACE::VSL_DEFAULT_dimension_STRUCT,
			 typename CoordinateInfo = AHO_NAMESPACE::coordinate::_DefaultCoordinateInfo<D>::value>
	struct PtrTriangle;

	template<typename R = int, 
			 VSL_NAMESPACE::is_dimension D = VSL_NAMESPACE::VSL_DEFAULT_dimension_STRUCT,
			 typename CoordinateInfo = AHO_NAMESPACE::coordinate::_DefaultCoordinateInfo<D>::value,
			 typename Size = size_t, 
			 typename Alloc = std::vector<IndexedTriangle<Size>>::allocator_type>
	struct Polygon;
}