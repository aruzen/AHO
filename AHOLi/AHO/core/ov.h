#pragma once
#include "../define.h"

#include "../../VSL/dimention.h"

#include "index.h"

#include <vector>

namespace AHO_NAMESPACE {
	template<typename R = int, VSL_NAMESPACE::is_dimention D = VSL_NAMESPACE::VSL_DEFAULT_DIMENTION_STRUCT>
	struct Line;

	template<typename R = int, VSL_NAMESPACE::is_dimention D = VSL_NAMESPACE::VSL_DEFAULT_DIMENTION_STRUCT>
	struct PtrLine;

	template<typename R = int, VSL_NAMESPACE::is_dimention D = VSL_NAMESPACE::VSL_DEFAULT_DIMENTION_STRUCT>
	struct Triangle; // points_object

	template<typename R = int, VSL_NAMESPACE::is_dimention D = VSL_NAMESPACE::VSL_DEFAULT_DIMENTION_STRUCT>
	struct PtrTriangle;

	template<typename R = int, VSL_NAMESPACE::is_dimention D = VSL_NAMESPACE::VSL_DEFAULT_DIMENTION_STRUCT, typename Size = size_t, typename Alloc = std::allocator<IndexedTriangle<Size>>>
	struct Polygon;
}