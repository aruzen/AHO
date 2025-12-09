#pragma once
#include "../define.hpp"

#include "VSL/dimension.hpp"

#include "vector.hpp"
#include "point.hpp"

#include "../utils/polygon.hpp"

#include <array>

namespace AHO_NAMESPACE {
	template<typename R, VSL_NAMESPACE::is_dimension D, typename CI>
	struct Line {
		using dimension = D;
		using element_type = R;
		using coordinate_info = CI;

		AHO_NAMESPACE::_Point<R, D, CI> pos1, pos2;

		constexpr Line() {}

		constexpr Line(AHO_NAMESPACE::_Point<R, D, CI> pos1, AHO_NAMESPACE::_Point<R, D, CI> pos2)
			: pos1(pos1), pos2(pos2) {}

		constexpr Triangle<R, D, CI> operator +(AHO_NAMESPACE::_Point<R, D, CI> t) const;

		template<AHO_NAMESPACE::concepts::points_view_able T>
			requires (!AHO_NAMESPACE::concepts::is_point<T>)
		constexpr Polygon<R, D, CI> operator +(T t) const;

		constexpr auto length() const;

		constexpr std::array<AHO_NAMESPACE::_Point<R, D, CI>, 2> points() const;

		constexpr std::array<Line<R, D, CI>, 1> lines() const;

		constexpr std::array<AHO_NAMESPACE::_Point<R, D, CI>*, 2> ptrpoints();

		constexpr std::array<PtrLine<R, D, CI>, 1> ptrlines();

		template<concepts::cast_as<R> T>
		constexpr Line<T, D, CI> cast();

		constexpr PtrLine<R, D, CI> adress();
	};

	template<typename R, VSL_NAMESPACE::is_dimension D, typename CI>
	struct PtrLine {
		using dimension = D;
		using element_type = R;
		using coordinate_info = CI;

		AHO_NAMESPACE::_Point<R, D, CI>* pos1,* pos2;

		constexpr PtrLine() {}

		constexpr PtrLine(AHO_NAMESPACE::_Point<R, D, CI>* pos1, AHO_NAMESPACE::_Point<R, D, CI>* pos2)
			: pos1(pos1), pos2(pos2) {}

		constexpr Triangle<R, D, CI> operator +(AHO_NAMESPACE::_Point<R, D, CI> t) const;

		template<AHO_NAMESPACE::concepts::points_view_able T>
		constexpr Polygon<R, D, CI> operator +(T t) const;

		constexpr auto length() const;

		constexpr std::array<AHO_NAMESPACE::_Point<R, D, CI>, 2> points() const;

		constexpr std::array<Line<R, D, CI>, 1> lines() const;

		constexpr std::array<AHO_NAMESPACE::_Point<R, D, CI>*, 2> ptrpoints() const;

		constexpr std::array<PtrLine<R, D, CI>, 1> ptrlines() const;

		constexpr Line<R, D, CI> indirect() const;
	};

	// --------------------------------------------------------------------------------------------------------------------

	template<typename R, VSL_NAMESPACE::is_dimension D, typename CI>
	inline constexpr Triangle<R, D, CI> Line<R, D, CI>::operator+(AHO_NAMESPACE::_Point<R, D, CI> t) const
	{
		return Triangle<R, D, CI>(pos1, pos2, t);
	}

	template<typename R, VSL_NAMESPACE::is_dimension D, typename CI>
	template<AHO_NAMESPACE::concepts::points_view_able T>
		requires (!AHO_NAMESPACE::concepts::is_point<T>)
	inline constexpr Polygon<R, D, CI> Line<R, D, CI>::operator+(T t) const
	{
		return Polygon<R, D, CI>();
	}

	template<typename R, VSL_NAMESPACE::is_dimension D, typename CI>
	inline constexpr auto Line<R, D, CI>::length() const {
		return (pos1 - pos2).length();
	};

	template<typename R, VSL_NAMESPACE::is_dimension D, typename CI>
	inline constexpr std::array<AHO_NAMESPACE::_Point<R, D, CI>, 2> Line<R, D, CI>::points() const
	{
		return { pos1, pos2 };
	}
	template<typename R, VSL_NAMESPACE::is_dimension D, typename CI>
	inline constexpr std::array<Line<R, D, CI>, 1> Line<R, D, CI>::lines() const
	{
		return { *this };
	}

	template<typename R, VSL_NAMESPACE::is_dimension D, typename CI>
	inline constexpr std::array<AHO_NAMESPACE::_Point<R, D, CI>*, 2> Line<R, D, CI>::ptrpoints()
	{
		return std::array<AHO_NAMESPACE::_Point<R, D, CI>*, 2>(&pos1, &pos2);
	}

	template<typename R, VSL_NAMESPACE::is_dimension D, typename CI>
	inline constexpr std::array<PtrLine<R, D, CI>, 1> Line<R, D, CI>::ptrlines()
	{
		return { adress() };
	}

	template<typename R, VSL_NAMESPACE::is_dimension D, typename CI>
	template<concepts::cast_as<R> T>
	inline constexpr Line<T, D, CI> Line<R, D, CI>::cast()
	{
		return Line<T, D, CI>(pos1.template cast<T>(), pos2.template cast<T>());
	}

	template<typename R, VSL_NAMESPACE::is_dimension D, typename CI>
	inline constexpr PtrLine<R, D, CI> Line<R, D, CI>::adress()
	{
		return PtrLine<R, D, CI>(&pos1, &pos2);
	}

	// --------------------------------------------------------------------------------------------------------------------

	template<typename R, VSL_NAMESPACE::is_dimension D, typename CI>
	inline constexpr Triangle<R, D, CI> PtrLine<R, D, CI>::operator+(AHO_NAMESPACE::_Point<R, D, CI> t) const
	{
		return Triangle<R, D, CI>(*pos1, *pos2, t);
	}

	template<typename R, VSL_NAMESPACE::is_dimension D, typename CI>
	template<AHO_NAMESPACE::concepts::points_view_able T>
	inline constexpr Polygon<R, D, CI> PtrLine<R, D, CI>::operator+(T t) const
	{
		return Polygon<R, D, CI>();
	}

	template<typename R, VSL_NAMESPACE::is_dimension D, typename CI>
	inline constexpr auto PtrLine<R, D, CI>::length() const {
		return (*pos1 - *pos2).length();
	};

	template<typename R, VSL_NAMESPACE::is_dimension D, typename CI>
	inline constexpr std::array<AHO_NAMESPACE::_Point<R, D, CI>, 2> PtrLine<R, D, CI>::points() const
	{
		return { *pos1, *pos2 };
	}

	template<typename R, VSL_NAMESPACE::is_dimension D, typename CI>
	inline constexpr std::array<Line<R, D, CI>, 1> PtrLine<R, D, CI>::lines() const
	{
		return { indirect() };
	}

	template<typename R, VSL_NAMESPACE::is_dimension D, typename CI>
	inline constexpr std::array<AHO_NAMESPACE::_Point<R, D, CI>*, 2> PtrLine<R, D, CI>::ptrpoints() const
	{
		return std::array<AHO_NAMESPACE::_Point<R, D, CI>*, 2>(pos1, pos2);
	}

	template<typename R, VSL_NAMESPACE::is_dimension D, typename CI>
	inline constexpr std::array<PtrLine<R, D, CI>, 1> PtrLine<R, D, CI>::ptrlines() const
	{
		return { *this };
	}

	template<typename R, VSL_NAMESPACE::is_dimension D, typename CI>
	inline constexpr Line<R, D, CI> PtrLine<R, D, CI>::indirect() const
	{
		return Line<R, D, CI>(*pos1, *pos2);
	}


    static_assert(concepts::is_line<Line<double, VSL_NAMESPACE::D2, coordinate::_DefaultCoordinateInfo<VSL_NAMESPACE::D2>::value>>);
    static_assert(concepts::is_points_object<Line<double, VSL_NAMESPACE::D2, coordinate::_DefaultCoordinateInfo<VSL_NAMESPACE::D2>::value>>);
}