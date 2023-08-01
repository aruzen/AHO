#pragma once
#include "../define.h"

#include "../../VSL/dimention.h"

#include "Vector.h"
#include "Point.h"

#include "../utils/polygon.h"

#include "Line.h"

#include <array>

namespace AHO_NAMESPACE {
	template<typename R, VSL_NAMESPACE::is_dimention D, typename CI>
	struct Triangle {
		using dimention = D;
		using element_type = R;

		AHO_NAMESPACE::_Point<R, D, CI> pos1, pos2, pos3;

		constexpr Triangle() {}

		constexpr Triangle(AHO_NAMESPACE::_Point<R, D, CI> pos1, AHO_NAMESPACE::_Point<R, D, CI> pos2, AHO_NAMESPACE::_Point<R, D, CI> pos3)
			: pos1(pos1), pos2(pos2), pos3(pos3) {}

		template<AHO_NAMESPACE::concepts::points_view_able T>
		constexpr Polygon<R, D, CI> operator +(T t) const;

		constexpr double area() const;

		constexpr std::array<AHO_NAMESPACE::_Point<R, D, CI>, 3> points() const;

		constexpr std::array<AHO_NAMESPACE::Line<R, D, CI>, 3> lines() const;

		constexpr std::array<Triangle<R, D, CI>, 1> triangles() const;

		constexpr std::array<AHO_NAMESPACE::_Point<R, D, CI>*, 3> ptrpoints();

		constexpr std::array<AHO_NAMESPACE::PtrLine<R, D, CI>, 3> ptrlines();

		constexpr std::array<PtrTriangle<R, D, CI>, 1> ptrtriangles();

		constexpr PtrTriangle<R, D, CI> adress();
	};

	template<typename R, VSL_NAMESPACE::is_dimention D, typename CI>
	struct PtrTriangle {
		using dimention = D;
		using element_type = R;

		AHO_NAMESPACE::_Point<R, D, CI>* pos1, pos2, pos3;

		constexpr PtrTriangle() {}

		constexpr PtrTriangle(AHO_NAMESPACE::_Point<R, D, CI>* pos1, AHO_NAMESPACE::_Point<R, D, CI>* pos2, AHO_NAMESPACE::_Point<R, D, CI>* pos3)
			: pos1(pos1), pos2(pos2), pos3(pos3) {}

		template<AHO_NAMESPACE::concepts::points_view_able T>
		constexpr Polygon<R, D, CI> operator +(T t) const;

		constexpr double area() const;

		constexpr std::array<AHO_NAMESPACE::_Point<R, D, CI>, 3> points() const;

		constexpr std::array<AHO_NAMESPACE::Line<R, D, CI>, 3> lines() const;

		constexpr std::array<Triangle<R, D, CI>, 1> triangles() const;

		constexpr std::array<AHO_NAMESPACE::_Point<R, D, CI>*, 3> ptrpoints() const;

		constexpr std::array<AHO_NAMESPACE::PtrLine<R, D, CI>, 3> ptrlines()  const;

		constexpr std::array<PtrTriangle<R, D, CI>, 1> ptrtriangles() const;

		constexpr Triangle<R, D, CI> inderect() const;
	};

	// --------------------------------------------------------------------------------------------------------------------

	template<typename R, VSL_NAMESPACE::is_dimention D, typename CI>
	template<AHO_NAMESPACE::concepts::points_view_able T>
	inline constexpr Polygon<R, D, CI> Triangle<R, D, CI>::operator+(T t) const
	{
		return Polygon<R, D, CI>();
	}

	template<typename R, VSL_NAMESPACE::is_dimention D, typename CI>
	inline constexpr double Triangle<R, D, CI>::area() const
	{
		auto v1 = pos2 - pos1;
		auto v2 = pos3 - pos1;
		return 0.5 * (v1 * v2);
	}

	template<typename R, VSL_NAMESPACE::is_dimention D, typename CI>
	inline constexpr std::array<AHO_NAMESPACE::_Point<R, D, CI>, 3> Triangle<R, D, CI>::points() const
	{
		return { pos1, pos2, pos3 };
	}

	template<typename R, VSL_NAMESPACE::is_dimention D, typename CI>
	inline constexpr std::array<AHO_NAMESPACE::Line<R, D, CI>, 3> Triangle<R, D, CI>::lines() const
	{
		return std::array<AHO_NAMESPACE::Line<R, D, CI>, 3>({ pos1, pos2 }, { pos2, pos3 }, { pos3, pos1 });
	}

	template<typename R, VSL_NAMESPACE::is_dimention D, typename CI>
	inline constexpr std::array<Triangle<R, D, CI>, 1> Triangle<R, D, CI>::triangles() const
	{
		return std::array<Triangle<R, D, CI>, 1>( *this );
	}

	template<typename R, VSL_NAMESPACE::is_dimention D, typename CI>
	inline constexpr std::array<AHO_NAMESPACE::_Point<R, D, CI>*, 3> Triangle<R, D, CI>::ptrpoints()
	{
		return std::array<AHO_NAMESPACE::_Point<R, D, CI>*, 3>(&pos1, &pos2, &pos3);
	}

	template<typename R, VSL_NAMESPACE::is_dimention D, typename CI>
	inline constexpr std::array<AHO_NAMESPACE::PtrLine<R, D, CI>, 3> Triangle<R, D, CI>::ptrlines()
	{
		return std::array<AHO_NAMESPACE::PtrLine<R, D, CI>, 3>({ &pos1, &pos2 }, { &pos2, &pos3 }, { &pos3, &pos1 });
	}

	template<typename R, VSL_NAMESPACE::is_dimention D, typename CI>
	inline constexpr std::array<PtrTriangle<R, D, CI>, 1> Triangle<R, D, CI>::ptrtriangles()
	{
		return std::array<PtrTriangle<R, D, CI>, 1>(PtrTriangle(&pos1, &pos2, &pos3));
	}

	template<typename R, VSL_NAMESPACE::is_dimention D, typename CI>
	inline constexpr PtrTriangle<R, D, CI> Triangle<R, D, CI>::adress()
	{
		return PtrTriangle<R, D, CI>(&pos1, &pos2, &pos3);
	}

	// --------------------------------------------------------------------------------------------------------------------

	template<typename R, VSL_NAMESPACE::is_dimention D, typename CI>
	template<AHO_NAMESPACE::concepts::points_view_able T>
	inline constexpr Polygon<R, D, CI> PtrTriangle<R, D, CI>::operator+(T t) const
	{
		return Polygon<R, D, CI>();
	}

	template<typename R, VSL_NAMESPACE::is_dimention D, typename CI>
	inline constexpr double PtrTriangle<R, D, CI>::area() const
	{
		auto v1 = pos2 - pos1;
		auto v2 = pos3 - pos1;
		return 0.5 * (v1 * v2);
	}

	template<typename R, VSL_NAMESPACE::is_dimention D, typename CI>
	inline constexpr std::array<AHO_NAMESPACE::_Point<R, D, CI>, 3> PtrTriangle<R, D, CI>::points() const
	{
		return { *pos1, *pos2, *pos3 };
	}

	template<typename R, VSL_NAMESPACE::is_dimention D, typename CI>
	inline constexpr std::array<AHO_NAMESPACE::Line<R, D, CI>, 3> PtrTriangle<R, D, CI>::lines() const
	{
		return std::array<AHO_NAMESPACE::Line<R, D, CI>, 3>({ *pos1, *pos2 }, { *pos2, pos3 }, { *pos3, *pos1 });
	}

	template<typename R, VSL_NAMESPACE::is_dimention D, typename CI>
	inline constexpr std::array<Triangle<R, D, CI>, 1> PtrTriangle<R, D, CI>::triangles() const
	{
		return std::array<Triangle<R, D, CI>, 1>({ *pos1, *pos2, *pos3 });
	}

	template<typename R, VSL_NAMESPACE::is_dimention D, typename CI>
	inline constexpr std::array<AHO_NAMESPACE::_Point<R, D, CI>*, 3> PtrTriangle<R, D, CI>::ptrpoints() const
	{
		return std::array<AHO_NAMESPACE::_Point<R, D, CI>*, 3>(pos1, pos2, pos3);
	}

	template<typename R, VSL_NAMESPACE::is_dimention D, typename CI>
	inline constexpr std::array<AHO_NAMESPACE::PtrLine<R, D, CI>, 3> PtrTriangle<R, D, CI>::ptrlines() const
	{
		return std::array<AHO_NAMESPACE::PtrLine<R, D, CI>, 3>({ pos1, pos2 }, { pos2, pos3 }, { pos3, pos1 });
	}

	template<typename R, VSL_NAMESPACE::is_dimention D, typename CI>
	inline constexpr std::array<PtrTriangle<R, D, CI>, 1> PtrTriangle<R, D, CI>::ptrtriangles() const
	{
		return std::array<PtrTriangle<R, D, CI>, 1>({ *this });
	}

	template<typename R, VSL_NAMESPACE::is_dimention D, typename CI>
	inline constexpr Triangle<R, D, CI> PtrTriangle<R, D, CI>::inderect() const
	{
		return Triangle<R, D, CI>(*pos1, *pos2, *pos3);
	}
}