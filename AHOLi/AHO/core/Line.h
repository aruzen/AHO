#pragma once
#include "../define.h"

#include "../../VSL/dimention.h"

#include "Vector.h"
#include "Point.h"

#include "../utils/polygon.h"

#include <array>

namespace AHO_NAMESPACE {
	template<typename R, VSL_NAMESPACE::is_dimention D>
	struct Line {
		using dimention = D;
		using element_type = R;

		AHO_NAMESPACE::_Point<R, D> pos1, pos2;

		constexpr Line() {}

		constexpr Line(AHO_NAMESPACE::_Point<R, D> pos1, AHO_NAMESPACE::_Point<R, D> pos2)
			: pos1(pos1), pos2(pos2) {}

		constexpr Triangle<R, D> operator +(AHO_NAMESPACE::_Point<R, D> t) const;

		template<AHO_NAMESPACE::concepts::points_view_able T>
		constexpr Polygon<R, D> operator +(T t) const;

		constexpr std::array<AHO_NAMESPACE::_Point<R, D>, 2> points() const;

		constexpr std::array<Line<R, D>, 1> lines() const;

		constexpr std::array<AHO_NAMESPACE::_Point<R, D>*, 2> ptrpoints();

		constexpr std::array<PtrLine<R, D>, 1> ptrlines();

		constexpr PtrLine<R, D> adress();
	};

	template<typename R, VSL_NAMESPACE::is_dimention D>
	struct PtrLine {
		using dimention = D;
		using element_type = R;

		AHO_NAMESPACE::_Point<R, D>* pos1, pos2;

		constexpr PtrLine() {}

		constexpr PtrLine(AHO_NAMESPACE::_Point<R, D>* pos1, AHO_NAMESPACE::_Point<R, D>* pos2)
			: pos1(pos1), pos2(pos2) {}

		constexpr Triangle<R, D> operator +(AHO_NAMESPACE::_Point<R, D> t) const;

		template<AHO_NAMESPACE::concepts::points_view_able T>
		constexpr Polygon<R, D> operator +(T t) const;

		constexpr std::array<AHO_NAMESPACE::_Point<R, D>, 2> points() const;

		constexpr std::array<Line<R, D>, 1> lines() const;

		constexpr std::array<AHO_NAMESPACE::_Point<R, D>*, 2> ptrpoints() const;

		constexpr std::array<PtrLine<R, D>, 1> ptrlines() const;

		constexpr Line<R, D> indirect() const;
	};

	// --------------------------------------------------------------------------------------------------------------------

	template<typename R, VSL_NAMESPACE::is_dimention D>
	inline constexpr Triangle<R, D> Line<R, D>::operator+(AHO_NAMESPACE::_Point<R, D> t) const
	{
		return Triangle<R, D>(pos1, pos2, t);
	}

	template<typename R, VSL_NAMESPACE::is_dimention D>
	template<AHO_NAMESPACE::concepts::points_view_able T>
	inline constexpr Polygon<R, D> Line<R, D>::operator+(T t) const
	{
		return Polygon<R, D>();
	}

	template<typename R, VSL_NAMESPACE::is_dimention D>
	inline constexpr std::array<AHO_NAMESPACE::_Point<R, D>, 2> Line<R, D>::points() const
	{
		return { pos1, pos2 };
	}
	template<typename R, VSL_NAMESPACE::is_dimention D>
	inline constexpr std::array<Line<R, D>, 1> Line<R, D>::lines() const
	{
		return { *this };
	}

	template<typename R, VSL_NAMESPACE::is_dimention D>
	inline constexpr std::array<AHO_NAMESPACE::_Point<R, D>*, 2> Line<R, D>::ptrpoints()
	{
		return std::array<AHO_NAMESPACE::_Point<R, D>*, 2>(&pos1, &pos2);
	}

	template<typename R, VSL_NAMESPACE::is_dimention D>
	inline constexpr std::array<PtrLine<R, D>, 1> Line<R, D>::ptrlines()
	{
		return { adress() };
	}

	template<typename R, VSL_NAMESPACE::is_dimention D>
	inline constexpr PtrLine<R, D> Line<R, D>::adress()
	{
		return PtrLine<R, D>(&pos1, &pos2);
	}

	// --------------------------------------------------------------------------------------------------------------------

	template<typename R, VSL_NAMESPACE::is_dimention D>
	inline constexpr Triangle<R, D> PtrLine<R, D>::operator+(AHO_NAMESPACE::_Point<R, D> t) const
	{
		return Triangle<R, D>(*pos1, *pos2, t);
	}

	template<typename R, VSL_NAMESPACE::is_dimention D>
	template<AHO_NAMESPACE::concepts::points_view_able T>
	inline constexpr Polygon<R, D> PtrLine<R, D>::operator+(T t) const
	{
		return Polygon<R, D>();
	}

	template<typename R, VSL_NAMESPACE::is_dimention D>
	inline constexpr std::array<AHO_NAMESPACE::_Point<R, D>, 2> PtrLine<R, D>::points() const
	{
		return { *pos1, *pos2 };
	}
	template<typename R, VSL_NAMESPACE::is_dimention D>
	inline constexpr std::array<Line<R, D>, 1> PtrLine<R, D>::lines() const
	{
		return { indirect() };
	}

	template<typename R, VSL_NAMESPACE::is_dimention D>
	inline constexpr std::array<AHO_NAMESPACE::_Point<R, D>*, 2> PtrLine<R, D>::ptrpoints() const
	{
		return std::array<AHO_NAMESPACE::_Point<R, D>*, 2>(pos1, pos2);
	}

	template<typename R, VSL_NAMESPACE::is_dimention D>
	inline constexpr std::array<PtrLine<R, D>, 1> PtrLine<R, D>::ptrlines() const
	{
		return { *this };
	}
	template<typename R, VSL_NAMESPACE::is_dimention D>
	inline constexpr Line<R, D> PtrLine<R, D>::indirect() const
	{
		return Line<R, D>(*pos1, *pos2);
	}
}