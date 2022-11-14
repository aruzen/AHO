#pragma once
#include "../define.h"

#include "../../VSL/dimention.h"

#include "Vector.h"
#include "Point.h"

#include "../utils/polygon.h"

#include "Line.h"

#include <array>

namespace AHO_NAMESPACE {
	template<typename R, VSL_NAMESPACE::is_dimention D>
	struct Triangle {
		using dimention = D;
		using element_type = R;

		AHO_NAMESPACE::_Point<R, D> pos1, pos2, pos3;

		constexpr Triangle() {}

		constexpr Triangle(AHO_NAMESPACE::_Point<R, D> pos1, AHO_NAMESPACE::_Point<R, D> pos2, AHO_NAMESPACE::_Point<R, D> pos3)
			: pos1(pos1), pos2(pos2), pos3(pos3) {}

		template<AHO_NAMESPACE::points_view_able T>
		constexpr Polygon<R, D> operator +(T t) const;

		constexpr double area() const;

		constexpr std::array<AHO_NAMESPACE::_Point<R, D>, 3> points() const;

		constexpr std::array<AHO_NAMESPACE::Line<R, D>, 3> lines() const;

		constexpr std::array<Triangle<R, D>, 1> triangles() const;

		constexpr std::array<AHO_NAMESPACE::_Point<R, D>*, 3> ptrpoints();

		constexpr std::array<AHO_NAMESPACE::PtrLine<R, D>, 3> ptrlines();

		constexpr std::array<PtrTriangle<R, D>, 1> ptrtriangles();

		constexpr PtrTriangle<R, D> adress();
	};

	template<typename R, VSL_NAMESPACE::is_dimention D>
	struct PtrTriangle {
		using dimention = D;
		using element_type = R;

		AHO_NAMESPACE::_Point<R, D>* pos1, pos2, pos3;

		constexpr PtrTriangle() {}

		constexpr PtrTriangle(AHO_NAMESPACE::_Point<R, D>* pos1, AHO_NAMESPACE::_Point<R, D>* pos2, AHO_NAMESPACE::_Point<R, D>* pos3)
			: pos1(pos1), pos2(pos2), pos3(pos3) {}

		template<AHO_NAMESPACE::points_view_able T>
		constexpr Polygon<R, D> operator +(T t) const;

		constexpr double area() const;

		constexpr std::array<AHO_NAMESPACE::_Point<R, D>, 3> points() const;

		constexpr std::array<AHO_NAMESPACE::Line<R, D>, 3> lines() const;

		constexpr std::array<Triangle<R, D>, 1> triangles() const;

		constexpr std::array<AHO_NAMESPACE::_Point<R, D>*, 3> ptrpoints() const;

		constexpr std::array<AHO_NAMESPACE::PtrLine<R, D>, 3> ptrlines()  const;

		constexpr std::array<PtrTriangle<R, D>, 1> ptrtriangles() const;

		constexpr Triangle<R, D> inderect() const;
	};

	// --------------------------------------------------------------------------------------------------------------------

	template<typename R, VSL_NAMESPACE::is_dimention D>
	template<AHO_NAMESPACE::points_view_able T>
	inline constexpr Polygon<R, D> Triangle<R, D>::operator+(T t) const
	{
		return Polygon<R, D>();
	}

	template<typename R, VSL_NAMESPACE::is_dimention D>
	inline constexpr double Triangle<R, D>::area() const
	{
		auto v1 = pos2 - pos1;
		auto v2 = pos3 - pos1;
		return 0.5 * (v1 * v2);
	}

	template<typename R, VSL_NAMESPACE::is_dimention D>
	inline constexpr std::array<AHO_NAMESPACE::_Point<R, D>, 3> Triangle<R, D>::points() const
	{
		return { pos1, pos2, pos3 };
	}

	template<typename R, VSL_NAMESPACE::is_dimention D>
	inline constexpr std::array<AHO_NAMESPACE::Line<R, D>, 3> Triangle<R, D>::lines() const
	{
		return std::array<AHO_NAMESPACE::Line<R, D>, 3>({ pos1, pos2 }, { pos2, pos3 }, { pos3, pos1 });
	}

	template<typename R, VSL_NAMESPACE::is_dimention D>
	inline constexpr std::array<Triangle<R, D>, 1> Triangle<R, D>::triangles() const
	{
		return std::array<Triangle<R, D>, 1>( *this );
	}

	template<typename R, VSL_NAMESPACE::is_dimention D>
	inline constexpr std::array<AHO_NAMESPACE::_Point<R, D>*, 3> Triangle<R, D>::ptrpoints()
	{
		return std::array<AHO_NAMESPACE::_Point<R, D>*, 3>(&pos1, &pos2, &pos3);
	}

	template<typename R, VSL_NAMESPACE::is_dimention D>
	inline constexpr std::array<AHO_NAMESPACE::PtrLine<R, D>, 3> Triangle<R, D>::ptrlines()
	{
		return std::array<AHO_NAMESPACE::PtrLine<R, D>, 3>({ &pos1, &pos2 }, { &pos2, &pos3 }, { &pos3, &pos1 });
	}

	template<typename R, VSL_NAMESPACE::is_dimention D>
	inline constexpr std::array<PtrTriangle<R, D>, 1> Triangle<R, D>::ptrtriangles()
	{
		return std::array<PtrTriangle<R, D>, 1>(PtrTriangle(&pos1, &pos2, &pos3));
	}

	template<typename R, VSL_NAMESPACE::is_dimention D>
	inline constexpr PtrTriangle<R, D> Triangle<R, D>::adress()
	{
		return PtrTriangle<R, D>(&pos1, &pos2, &pos3);
	}

	// --------------------------------------------------------------------------------------------------------------------

	template<typename R, VSL_NAMESPACE::is_dimention D>
	template<AHO_NAMESPACE::points_view_able T>
	inline constexpr Polygon<R, D> PtrTriangle<R, D>::operator+(T t) const
	{
		return Polygon<R, D>();
	}

	template<typename R, VSL_NAMESPACE::is_dimention D>
	inline constexpr double PtrTriangle<R, D>::area() const
	{
		auto v1 = pos2 - pos1;
		auto v2 = pos3 - pos1;
		return 0.5 * (v1 * v2);
	}

	template<typename R, VSL_NAMESPACE::is_dimention D>
	inline constexpr std::array<AHO_NAMESPACE::_Point<R, D>, 3> PtrTriangle<R, D>::points() const
	{
		return { *pos1, *pos2, *pos3 };
	}

	template<typename R, VSL_NAMESPACE::is_dimention D>
	inline constexpr std::array<AHO_NAMESPACE::Line<R, D>, 3> PtrTriangle<R, D>::lines() const
	{
		return std::array<AHO_NAMESPACE::Line<R, D>, 3>({ *pos1, *pos2 }, { *pos2, pos3 }, { *pos3, *pos1 });
	}

	template<typename R, VSL_NAMESPACE::is_dimention D>
	inline constexpr std::array<Triangle<R, D>, 1> PtrTriangle<R, D>::triangles() const
	{
		return std::array<Triangle<R, D>, 1>({ *pos1, *pos2, *pos3 });
	}

	template<typename R, VSL_NAMESPACE::is_dimention D>
	inline constexpr std::array<AHO_NAMESPACE::_Point<R, D>*, 3> PtrTriangle<R, D>::ptrpoints() const
	{
		return std::array<AHO_NAMESPACE::_Point<R, D>*, 3>(pos1, pos2, pos3);
	}

	template<typename R, VSL_NAMESPACE::is_dimention D>
	inline constexpr std::array<AHO_NAMESPACE::PtrLine<R, D>, 3> PtrTriangle<R, D>::ptrlines() const
	{
		return std::array<AHO_NAMESPACE::PtrLine<R, D>, 3>({ pos1, pos2 }, { pos2, pos3 }, { pos3, pos1 });
	}

	template<typename R, VSL_NAMESPACE::is_dimention D>
	inline constexpr std::array<PtrTriangle<R, D>, 1> PtrTriangle<R, D>::ptrtriangles() const
	{
		return std::array<PtrTriangle<R, D>, 1>({ *this });
	}

	template<typename R, VSL_NAMESPACE::is_dimention D>
	inline constexpr Triangle<R, D> PtrTriangle<R, D>::inderect() const
	{
		return Triangle<R, D>(*pos1, *pos2, *pos3);
	}
}