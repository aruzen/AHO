#pragma once
#include "VSL/dimension.hpp"

#include "../../define.hpp"
#include "../../concepts.hpp"

#include <iostream>

namespace AHO_NAMESPACE {
	namespace coordinate {
#define COORDINATE_ELEMENT_TYPE_DEFINITION_EXPANDER(Upper, Lower) \
        template <typename T> \
        struct Upper { \
            T value; \
         \
            constexpr static char UpperChar = #Upper[0]; \
            constexpr static char LowerChar = #Lower[0]; \
         \
            constexpr Upper(const T& t) : value(t) {}; \
            constexpr Upper() : value{} {};                                  \
         \
            constexpr auto operator<=>(const Upper<T>&) const = default;\
         \
            constexpr Upper<T> operator +() const { static_assert(concepts::plusable<T>); return { value }; }; \
            constexpr Upper<T> operator -() const { static_assert(concepts::minusable<T>); return { -value }; }; \
            template <concepts::add_as<T> R> \
            constexpr Upper<decltype(std::declval<T>() + std::declval<R>())> operator +(const Upper<R>& t) const { return { value + t.value }; }; \
            template <concepts::subtract_as<T> R> \
            constexpr Upper<decltype(std::declval<T>() - std::declval<R>())> operator -(const Upper<R>& t) const { return { value - t.value }; }; \
            template <concepts::multiply_as<T> R> \
            constexpr Upper<decltype(std::declval<T>()* std::declval<R>())> operator *(const Upper<R>& t) const { return { value * t.value }; }; \
            template <concepts::division_as<T> R> \
            constexpr Upper<decltype(std::declval<T>() / std::declval<R>())> operator /(const Upper<R>& t) const { return { value / t.value }; }; \
         \
            template <concepts::add_as<T> R> \
            constexpr Upper<T>& operator +=(const Upper<R>& t) { value += t.value; return *this; }; \
            template <concepts::subtract_as<T> R> \
            constexpr Upper<T>& operator -=(const Upper<R>& t) { value -= t.value; return *this; }; \
            template <concepts::multiply_as<T> R> \
            constexpr Upper<T>& operator *=(const Upper<R>& t) { value *= t.value; return *this; }; \
            template <concepts::division_as<T> R> \
            constexpr Upper<T>& operator /=(const Upper<R>& t) { value /= t.value; return *this; }; \
         \
            template <concepts::cast_as<T> R> \
            Upper<R> cast() const { return { (R)value }; } \
        }; \
        template<typename T> \
        std::ostream& operator <<(std::ostream& out, const Upper<T>& Lower) { \
            out << #Lower << " : " << Lower .value; \
            return out; \
        };

		COORDINATE_ELEMENT_TYPE_DEFINITION_EXPANDER(X, x);
		COORDINATE_ELEMENT_TYPE_DEFINITION_EXPANDER(Y, y);
		COORDINATE_ELEMENT_TYPE_DEFINITION_EXPANDER(Z, z);

		template <bool ...Booleans>
		constexpr size_t _CountTruesFunc() {
			size_t n = 0;
			bool bools[] = { Booleans... };
			for (bool f : bools)
				if (f)
					n++;
			return n;
		}

		template <bool ...Booleans>
		struct _CountTrues {
			constexpr static size_t value = _CountTruesFunc<Booleans...>();
		};

		template<bool EnableX = false, bool EnableY = false, bool EnableZ = false>
		struct _MakeCoordinateInfo;

		template <VSL_NAMESPACE::is_dimension D, bool EnableX, bool EnableY = false, bool EnableZ = false>
		struct _CoordinateInfo {
			using dimension = D;
			constexpr static bool EnabledX = EnableX;
			constexpr static bool EnabledY = EnableY;
			constexpr static bool EnabledZ = EnableZ;
			constexpr static bool IndexX = _CountTruesFunc<EnableX>();
			constexpr static bool IndexY = _CountTruesFunc<EnableX, EnableY>();
			constexpr static bool IndexZ = _CountTruesFunc<EnableX, EnableY, EnableZ>();

			consteval static typename _MakeCoordinateInfo<true, EnableY, EnableZ>::value EnablingX() {};
			consteval static typename _MakeCoordinateInfo<EnableX, true, EnableZ>::value EnablingY() {};
			consteval static typename _MakeCoordinateInfo<EnableX, EnableY, true>::value EnablingZ() {};

			/*
			consteval static typename _MakeCoordinateInfo<false, EnableY, EnableZ>::value DisablingX() {};
			consteval static typename _MakeCoordinateInfo<EnableX, false, EnableZ>::value DisablingY() {};
			consteval static typename _MakeCoordinateInfo<EnableX, EnableY, false>::value DisablingZ() {};
			*/
		};

		template<bool EnableX, bool EnableY, bool EnableZ>
		struct _MakeCoordinateInfo {
			using value = _CoordinateInfo<typename VSL_NAMESPACE::dimension<_CountTrues<EnableX, EnableY, EnableZ>::value>::value, EnableX, EnableY, EnableZ>;
		};

		template<VSL_NAMESPACE::is_dimension D>
		struct _DefaultCoordinateInfo {};

		template<>
		struct _DefaultCoordinateInfo<VSL_NAMESPACE::D1> {
			using value = _CoordinateInfo<VSL_NAMESPACE::D1, true>;
		};

		template<>
		struct _DefaultCoordinateInfo<VSL_NAMESPACE::D2> {
			using value = _CoordinateInfo<VSL_NAMESPACE::D2, true, true>;
		};

		template<>
		struct _DefaultCoordinateInfo<VSL_NAMESPACE::D3> {
			using value = _CoordinateInfo<VSL_NAMESPACE::D3, true, true, true>;
		};

		template <typename ElementType, typename _CoordinateInfo>
		struct _CoordinateSet {
			static constexpr bool is_coordinate_set = true;
		};

#define AHO_COORDINATE_NUMBERD_MEMBER_1 ___AHO_JOIN(AHO_COORDINATE_NUMBERD_MEMBER_NAME, 1)
#define AHO_COORDINATE_NUMBERD_MEMBER_2 ___AHO_JOIN(AHO_COORDINATE_NUMBERD_MEMBER_NAME, 2)
#define AHO_COORDINATE_NUMBERD_MEMBER_3 ___AHO_JOIN(AHO_COORDINATE_NUMBERD_MEMBER_NAME, 3)

#ifdef _MSC_VER
#define ___AN1 ___AHO_JOIN(AHO_COORDINATE_NUMBERD_MEMBER_1, .value)
#define ___AN2 ___AHO_JOIN(AHO_COORDINATE_NUMBERD_MEMBER_2, .value)
#define ___AN3 ___AHO_JOIN(AHO_COORDINATE_NUMBERD_MEMBER_3, .value)
#elif __APPLE__
#define ___AN1 ___AHO_JOIN(AHO_COORDINATE_NUMBERD_MEMBER_1, ).value
#define ___AN2 ___AHO_JOIN(AHO_COORDINATE_NUMBERD_MEMBER_2, ).value
#define ___AN3 ___AHO_JOIN(AHO_COORDINATE_NUMBERD_MEMBER_3, ).value
#endif

#define AHO_COORDINATE_SET_OPERATOR_ADD_ASSIGN(Upper, Lower) \
				template <typename T> \
				constexpr _CoordinateSet<ElementType, coordinate_info>& operator +=(const Upper<T>& e) { Lower += e; return *this; } \
				template <typename T> \
				constexpr _CoordinateSet<ElementType, coordinate_info>& operator -=(const Upper<T>& e) { Lower -= e; return *this; } \

#define AHO_DEFINE_SPECIAL_COORDINATE_SET1(Upper, Lower) \
			template <typename ElementType> \
			struct _CoordinateSet<ElementType, typename _MakeCoordinateInfo< \
								  std::same_as<X<ElementType>, Upper<ElementType>>, \
								  std::same_as<Y<ElementType>, Upper<ElementType>>, \
								  std::same_as<Z<ElementType>, Upper<ElementType>>  \
							  >::value> { \
				using element_type = ElementType; \
				using coordinate_info = typename _MakeCoordinateInfo< \
								  std::same_as<X<ElementType>, Upper<ElementType>>, \
								  std::same_as<Y<ElementType>, Upper<ElementType>>, \
								  std::same_as<Z<ElementType>, Upper<ElementType>>  \
							  >::value; \
				static constexpr bool is_coordinate_set = true; \
				union { Upper<ElementType> Lower, AHO_COORDINATE_NUMBERD_MEMBER_1; }; \
				constexpr _CoordinateSet() : Lower(Upper<ElementType>()) {}; \
				constexpr _CoordinateSet(const Upper<ElementType>& Lower) : Lower(Lower) {}; \
                constexpr bool operator<(const _CoordinateSet<element_type, coordinate_info>& o) const { \
                    return this->_cnmn1 < o._cnmn1; \
                } \
			    constexpr auto operator<=>(const _CoordinateSet<element_type, coordinate_info>&) const = default; \
				AHO_COORDINATE_SET_OPERATOR_ADD_ASSIGN(Upper, Lower); \
			};

#define AHO_DEFINE_SPECIAL_COORDINATE_SET2(Upper1, Upper2, Lower1, Lower2) \
			template <typename ElementType> \
			struct _CoordinateSet<ElementType, typename _MakeCoordinateInfo< \
							      (std::same_as<X<ElementType>, Upper1<ElementType>> || std::same_as<X<ElementType>, Upper2<ElementType>>), \
							      (std::same_as<Y<ElementType>, Upper1<ElementType>> || std::same_as<Y<ElementType>, Upper2<ElementType>>), \
							      (std::same_as<Z<ElementType>, Upper1<ElementType>> || std::same_as<Z<ElementType>, Upper2<ElementType>>)  \
							  >::value> { \
				using element_type = ElementType; \
				using coordinate_info = typename _MakeCoordinateInfo< \
							      (std::same_as<X<ElementType>, Upper1<ElementType>> || std::same_as<X<ElementType>, Upper2<ElementType>>), \
							      (std::same_as<Y<ElementType>, Upper1<ElementType>> || std::same_as<Y<ElementType>, Upper2<ElementType>>), \
							      (std::same_as<Z<ElementType>, Upper1<ElementType>> || std::same_as<Z<ElementType>, Upper2<ElementType>>)  \
							  >::value; \
				static constexpr bool is_coordinate_set = true; \
				union { Upper1<ElementType> Lower1, AHO_COORDINATE_NUMBERD_MEMBER_1; }; \
				union { Upper2<ElementType> Lower2, AHO_COORDINATE_NUMBERD_MEMBER_2; }; \
				constexpr _CoordinateSet() : Lower1(Upper1<ElementType>()), Lower2(Upper2<ElementType>()) {}; \
				constexpr _CoordinateSet(const Upper1<ElementType>& Lower1, const Upper2<ElementType>& Lower2) : Lower1(Lower1), Lower2(Lower2) {}; \
                constexpr bool operator<(const _CoordinateSet<element_type, coordinate_info>& o) const { \
                    return (this->_cnmn1 != o._cnmn1 ? this->_cnmn1 < o._cnmn1 : this->_cnmn2 < o._cnmn2); \
                } \
                constexpr auto operator<=>(const _CoordinateSet<element_type, coordinate_info>&) const = default;   \
                AHO_COORDINATE_SET_OPERATOR_ADD_ASSIGN(Upper1, Lower1); \
				AHO_COORDINATE_SET_OPERATOR_ADD_ASSIGN(Upper2, Lower2); \
			};

		AHO_DEFINE_SPECIAL_COORDINATE_SET1(X, x);
		AHO_DEFINE_SPECIAL_COORDINATE_SET1(Y, y);
		AHO_DEFINE_SPECIAL_COORDINATE_SET1(Z, z);

		AHO_DEFINE_SPECIAL_COORDINATE_SET2(X, Y, x, y);
		AHO_DEFINE_SPECIAL_COORDINATE_SET2(X, Z, x, z);
		AHO_DEFINE_SPECIAL_COORDINATE_SET2(Y, Z, y, z);

		template <typename ElementType>
		struct _CoordinateSet<ElementType, typename _MakeCoordinateInfo<true, true, true>::value> {
			using element_type = ElementType;
			using coordinate_info = _MakeCoordinateInfo<true, true, true>::value;

			static constexpr bool is_coordinate_set = true;
			union { X<ElementType> x, AHO_COORDINATE_NUMBERD_MEMBER_1; };
			union { Y<ElementType> y, AHO_COORDINATE_NUMBERD_MEMBER_2; };
			union { Z<ElementType> z, AHO_COORDINATE_NUMBERD_MEMBER_3; };

			constexpr _CoordinateSet() : x(X<ElementType>()), y(Y<ElementType>()), z(Z<ElementType>()) {}
			constexpr _CoordinateSet(const X<ElementType>& x, const Y<ElementType>& y, const Z<ElementType>& z) : x(x), y(y), z(z) {}
            constexpr bool operator<(const _CoordinateSet<element_type, coordinate_info>& o) const {
                return (this->_cnmn1 != o._cnmn1 ? this->_cnmn1 < o._cnmn1 :
                        (this->_cnmn2 != o._cnmn2 ? this->_cnmn2 < o._cnmn2 :
                         this->_cnmn3 < o._cnmn3));
            }
            constexpr auto operator<=>(const _CoordinateSet<element_type, coordinate_info>&) const = default;

			AHO_COORDINATE_SET_OPERATOR_ADD_ASSIGN(X, x);
			AHO_COORDINATE_SET_OPERATOR_ADD_ASSIGN(Y, y);
			AHO_COORDINATE_SET_OPERATOR_ADD_ASSIGN(Z, z);
		};

        template <typename CI1, typename CI2>
		struct _SumCoordinateInfo {
			using value = typename _MakeCoordinateInfo<CI1::EnabledX || CI2::EnabledX, CI1::EnabledY || CI2::EnabledY, CI1::EnabledZ || CI2::EnabledZ>::value;
		};

		template<typename T1, typename CI1, typename T2, typename CI2>
		constexpr _CoordinateSet<decltype(std::declval<T1>() + std::declval<T2>()), typename _SumCoordinateInfo<CI1, CI2>::value> operator +(const _CoordinateSet<T1, CI1>& e1, const _CoordinateSet<T2, CI2>& e2) {
			_CoordinateSet<decltype(std::declval<T1>() + std::declval<T2>()), typename _SumCoordinateInfo<CI1, CI2>::value> result;
			if constexpr (CI1::EnabledX) {
				result += e1.x;
			}
			if constexpr (CI1::EnabledY) {
				result += e1.y;
			}
			if constexpr (CI1::EnabledZ) {
				result += e1.z;
			}

			if constexpr (CI2::EnabledX) {
				result += e2.x;
			}
			if constexpr (CI2::EnabledY) {
				result += e2.y;
			}
			if constexpr (CI2::EnabledZ) {
				result += e2.z;
			}

			return result;
		}

		template<typename T1, typename CI1, typename T2>
		constexpr _CoordinateSet<decltype(std::declval<T1>() + std::declval<T2>()), typename _SumCoordinateInfo<CI1, _MakeCoordinateInfo<true, false, false>::value>::value> operator +(const _CoordinateSet<T1, CI1>& e1, const X<T2>& e2) {
			_CoordinateSet<decltype(std::declval<T1>() + std::declval<T2>()), typename _SumCoordinateInfo<CI1, _MakeCoordinateInfo<true, false, false>::value>::value> result;
			if constexpr (CI1::EnabledX) {
				result += e1.x;
			}
			if constexpr (CI1::EnabledY) {
				result += e1.y;
			}
			if constexpr (CI1::EnabledZ) {
				result += e1.z;
			}

			result += e2;

			return result;
		}

		template<typename T1, typename CI1, typename T2>
		constexpr _CoordinateSet<decltype(std::declval<T1>() + std::declval<T2>()), typename _SumCoordinateInfo<CI1, _MakeCoordinateInfo<false, true, false>::value>::value> operator +(const _CoordinateSet<T1, CI1>& e1, const Y<T2>& e2) {
			_CoordinateSet<decltype(std::declval<T1>() + std::declval<T2>()), typename _SumCoordinateInfo<CI1, _MakeCoordinateInfo<false, true, false>::value>::value> result;
			if constexpr (CI1::EnabledX) {
				result += e1.x;
			}
			if constexpr (CI1::EnabledY) {
				result += e1.y;
			}
			if constexpr (CI1::EnabledZ) {
				result += e1.z;
			}

			result += e2;

			return result;
		}

		template<typename T1, typename CI1, typename T2>
		constexpr _CoordinateSet<decltype(std::declval<T1>() + std::declval<T2>()), typename _SumCoordinateInfo<CI1, _MakeCoordinateInfo<false, false, true>::value>::value> operator +(const _CoordinateSet<T1, CI1>& e1, const Z<T2>& e2) {
			_CoordinateSet<decltype(std::declval<T1>() + std::declval<T2>()), typename _SumCoordinateInfo<CI1, _MakeCoordinateInfo<false, false, true>::value>::value> result;
			if constexpr (CI1::EnabledX) {
				result += e1.x;
			}
			if constexpr (CI1::EnabledY) {
				result += e1.y;
			}
			if constexpr (CI1::EnabledZ) {
				result += e1.z;
			}

			result += e2;

			return result;
		}

		template<typename T1, typename T2>
		constexpr _CoordinateSet<decltype(std::declval<T1>() + std::declval<T2>()), typename _MakeCoordinateInfo<true, true, false>::value> operator +(const X<T1>& e1, const Y<T2>& e2) {
			return { e1 , e2 };
		}

		template<typename T1, typename T2>
		constexpr _CoordinateSet<decltype(std::declval<T1>() + std::declval<T2>()), typename _MakeCoordinateInfo<true, false, true>::value> operator +(const X<T1>& e1, const Z<T2>& e2) {
			return { e1 , e2 };
		}

		template<typename T1, typename T2>
		constexpr _CoordinateSet<decltype(std::declval<T1>() + std::declval<T2>()), typename _MakeCoordinateInfo<false, true, true>::value> operator +(const Y<T1>& e1, const Y<T2>& e2) {
			return { e1 , e2 };
		}

        template<typename T1, typename CI1, typename T2, typename CI2>
        constexpr _CoordinateSet<decltype(std::declval<T1>() - std::declval<T2>()), typename _SumCoordinateInfo<CI1, CI2>::value> operator -(const _CoordinateSet<T1, CI1>& e1, const _CoordinateSet<T2, CI2>& e2) {
            _CoordinateSet<decltype(std::declval<T1>() - std::declval<T2>()), typename _SumCoordinateInfo<CI1, CI2>::value> result;
            if constexpr (CI1::EnabledX) {
                result -= e1.x;
            }
            if constexpr (CI1::EnabledY) {
                result -= e1.y;
            }
            if constexpr (CI1::EnabledZ) {
                result -= e1.z;
            }

            if constexpr (CI2::EnabledX) {
                result -= e2.x;
            }
            if constexpr (CI2::EnabledY) {
                result -= e2.y;
            }
            if constexpr (CI2::EnabledZ) {
                result -= e2.z;
            }

            return result;
        }

        template<typename T1, typename CI1, typename T2>
        constexpr _CoordinateSet<decltype(std::declval<T1>() - std::declval<T2>()), typename _SumCoordinateInfo<CI1, _MakeCoordinateInfo<true, false, false>::value>::value> operator -(const _CoordinateSet<T1, CI1>& e1, const X<T2>& e2) {
            _CoordinateSet<decltype(std::declval<T1>() - std::declval<T2>()), typename _SumCoordinateInfo<CI1, _MakeCoordinateInfo<true, false, false>::value>::value> result;
            if constexpr (CI1::EnabledX) {
                result -= e1.x;
            }
            if constexpr (CI1::EnabledY) {
                result -= e1.y;
            }
            if constexpr (CI1::EnabledZ) {
                result -= e1.z;
            }

            result -= e2;

            return result;
        }

        template<typename T1, typename CI1, typename T2>
        constexpr _CoordinateSet<decltype(std::declval<T1>() - std::declval<T2>()), typename _SumCoordinateInfo<CI1, _MakeCoordinateInfo<false, true, false>::value>::value> operator -(const _CoordinateSet<T1, CI1>& e1, const Y<T2>& e2) {
            _CoordinateSet<decltype(std::declval<T1>() - std::declval<T2>()), typename _SumCoordinateInfo<CI1, _MakeCoordinateInfo<false, true, false>::value>::value> result;
            if constexpr (CI1::EnabledX) {
                result -= e1.x;
            }
            if constexpr (CI1::EnabledY) {
                result -= e1.y;
            }
            if constexpr (CI1::EnabledZ) {
                result -= e1.z;
            }

            result -= e2;

            return result;
        }

        template<typename T1, typename CI1, typename T2>
        constexpr _CoordinateSet<decltype(std::declval<T1>() - std::declval<T2>()), typename _SumCoordinateInfo<CI1, _MakeCoordinateInfo<false, false, true>::value>::value> operator -(const _CoordinateSet<T1, CI1>& e1, const Z<T2>& e2) {
            _CoordinateSet<decltype(std::declval<T1>() - std::declval<T2>()), typename _SumCoordinateInfo<CI1, _MakeCoordinateInfo<false, false, true>::value>::value> result;
            if constexpr (CI1::EnabledX) {
                result -= e1.x;
            }
            if constexpr (CI1::EnabledY) {
                result -= e1.y;
            }
            if constexpr (CI1::EnabledZ) {
                result -= e1.z;
            }

            result -= e2;

            return result;
        }

        template<typename T1, typename T2>
        constexpr _CoordinateSet<decltype(std::declval<T1>() - std::declval<T2>()), typename _MakeCoordinateInfo<true, true, false>::value> operator -(const X<T1>& e1, const Y<T2>& e2) {
            return { e1 , e2 };
        }

        template<typename T1, typename T2>
        constexpr _CoordinateSet<decltype(std::declval<T1>() - std::declval<T2>()), typename _MakeCoordinateInfo<true, false, true>::value> operator -(const X<T1>& e1, const Z<T2>& e2) {
            return { e1 , e2 };
        }

        template<typename T1, typename T2>
        constexpr _CoordinateSet<decltype(std::declval<T1>() - std::declval<T2>()), typename _MakeCoordinateInfo<false, true, true>::value> operator -(const Y<T1>& e1, const Y<T2>& e2) {
            return { e1 , e2 };
        }
        namespace literals {
		    constexpr X<double> operator AHO_LITERAL(x)(long double v) { return { (double)v }; };
		    constexpr X<int> operator AHO_LITERAL(x)(unsigned long long v) { return { (int)v }; };
		    constexpr X<float> operator AHO_LITERAL(f_x)(long double v) { return { (float)v }; };
		    constexpr X<size_t> operator AHO_LITERAL(l_x)(unsigned long long v) { return { v }; };
		    constexpr X<double> x(1.0);

		    constexpr Y<double> operator AHO_LITERAL(y)(long double v) { return { (double)v }; };
		    constexpr Y<int> operator AHO_LITERAL(y)(unsigned long long v) { return { (int)v }; };
		    constexpr Y<float> operator AHO_LITERAL(f_y)(long double v) { return { (float)v }; };
		    constexpr Y<size_t> operator AHO_LITERAL(l_y)(unsigned long long v) { return { v }; };
		    constexpr Y<double> y(1.0);

		    constexpr Z<double> operator AHO_LITERAL(z)(long double v) { return { (double)v }; };
		    constexpr Z<int> operator AHO_LITERAL(z)(unsigned long long v) { return { (int)v }; };
		    constexpr Z<float> operator AHO_LITERAL(f_z)(long double v) { return { (float)v }; };
		    constexpr Z<size_t> operator AHO_LITERAL(l_z)(unsigned long long v) { return { v }; };
		    constexpr Z<double> z(1.0);
        }
	}

	namespace concepts {
		template<typename R>
		concept is_coordinate_set = requires {
			std::enable_if<R::is_coordinate_set>::type;
		};
	}

	namespace literals {
		using namespace coordinate::literals;
	}

	template <typename ElementType, typename CoordinationInfo>
	std::ostream& operator <<(std::ostream& out, const coordinate::_CoordinateSet<ElementType, CoordinationInfo>& set) {
		if constexpr (std::same_as<typename CoordinationInfo::dimension, VSL_NAMESPACE::D1>) {
			out << set.AHO_COORDINATE_NUMBERD_MEMBER_1;
		}

		if constexpr (std::same_as<typename CoordinationInfo::dimension, VSL_NAMESPACE::D2>) {
			out << "(" << decltype(set.AHO_COORDINATE_NUMBERD_MEMBER_1)::LowerChar
				<< ", " << decltype(set.AHO_COORDINATE_NUMBERD_MEMBER_2)::LowerChar << ")"
				<< "="
				<< "(" << set.AHO_COORDINATE_NUMBERD_MEMBER_1.value
				<< ", " << set.AHO_COORDINATE_NUMBERD_MEMBER_2.value << ")";
		}

		if constexpr (std::same_as<typename CoordinationInfo::dimension, VSL_NAMESPACE::D3>) {
            out << "(" << decltype(set.AHO_COORDINATE_NUMBERD_MEMBER_1)::LowerChar
                << ", " << decltype(set.AHO_COORDINATE_NUMBERD_MEMBER_2)::LowerChar
                << ", " << decltype(set.AHO_COORDINATE_NUMBERD_MEMBER_3)::LowerChar << ")"
                << "="
                << "(" << set.AHO_COORDINATE_NUMBERD_MEMBER_1.value
                << ", " << set.AHO_COORDINATE_NUMBERD_MEMBER_2.value
                << ", " << set.AHO_COORDINATE_NUMBERD_MEMBER_3.value << ")";
        }
        return out;
    };

    using coordinate::X;
    using coordinate::Y;
    using coordinate::Z;
}