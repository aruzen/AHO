#pragma once
#include "VSL/dimension.hpp"

#include "../../define.hpp"
#include "../../concepts.hpp"

#include <compare>
#include <tuple>
#include <iostream>
#include <cstddef>

namespace AHO_NAMESPACE {
    namespace coordinate {
#define COORDINATE_ELEMENT_TYPE_DEFINITION_EXPANDER(Upper, Lower)                               \
    template<typename T>                                                                        \
    struct Upper {                                                                              \
        T value;                                                                                \
                                                                                                \
        constexpr static char UPPER_CHAR = #Upper[0];                                           \
        constexpr static char LOWER_CHAR = #Lower[0];                                           \
                                                                                                \
        constexpr Upper(const T& t) : value(t) {};                                              \
        constexpr Upper() : value{} {};                                                         \
                                                                                                \
        constexpr auto operator<=>(const Upper<T>&) const = default;                            \
                                                                                                \
        constexpr Upper<T> operator+() const {                                                  \
            static_assert(concepts::plusable<T>);                                               \
            return {value};                                                                     \
        };                                                                                      \
        constexpr Upper<T> operator-() const {                                                  \
            static_assert(concepts::minusable<T>);                                              \
            return Upper<T>(-value);                                                            \
        };                                                                                      \
        template<typename R>                                                                    \
        requires concepts::add_as<const T&, const R&>                                           \
        constexpr auto operator+(const Upper<R>& t) const -> Upper<decltype(value + t.value)> { \
            return {value + t.value};                                                           \
        };                                                                                      \
        template<typename R>                                                                    \
        requires concepts::subtract_as<const T&, const R&>                                      \
        constexpr auto operator-(const Upper<R>& t) const -> Upper<decltype(value - t.value)> { \
            return {value - t.value};                                                           \
        };                                                                                      \
        template<typename R>                                                                    \
        requires concepts::multiply_as<const T&, const R&>                                      \
        constexpr auto operator*(const Upper<R>& t) const -> Upper<decltype(value * t.value)> { \
            return {value * t.value};                                                           \
        };                                                                                      \
        template<typename R>                                                                    \
        requires concepts::division_as<const T&, const R&>                                      \
        constexpr auto operator/(const Upper<R>& t) const -> Upper<decltype(value / t.value)> { \
            return {value / t.value};                                                           \
        };                                                                                      \
                                                                                                \
        template<typename R>                                                                    \
        requires concepts::add_assign_as<T&, const R&>                                          \
        constexpr Upper<T>& operator+=(const Upper<R>& t) {                                     \
            value += t.value;                                                                   \
            return *this;                                                                       \
        };                                                                                      \
        template<typename R>                                                                    \
        requires concepts::subtract_assign_as<T&, const R&>                                     \
        constexpr Upper<T>& operator-=(const Upper<R>& t) {                                     \
            value -= t.value;                                                                   \
            return *this;                                                                       \
        };                                                                                      \
        template<typename R>                                                                    \
        requires concepts::multiply_assign_as<T&, const R&>                                     \
        constexpr Upper<T>& operator*=(const Upper<R>& t) {                                     \
            value *= t.value;                                                                   \
            return *this;                                                                       \
        };                                                                                      \
        template<typename R>                                                                    \
        requires concepts::division_assign_as<T&, const R&>                                     \
        constexpr Upper<T>& operator/=(const Upper<R>& t) {                                     \
            value /= t.value;                                                                   \
            return *this;                                                                       \
        };                                                                                      \
                                                                                                \
        template<typename R>                                                                    \
        requires concepts::cast_as<T, R>                                                        \
        Upper<R> cast() const {                                                                 \
            return {(R)value};                                                                  \
        }                                                                                       \
    };                                                                                          \
    template<typename T>                                                                        \
    std::ostream& operator<<(std::ostream& out, const Upper<T>& Lower) {                        \
        out << #Lower << " : " << Lower.value;                                                  \
        return out;                                                                             \
    };

        COORDINATE_ELEMENT_TYPE_DEFINITION_EXPANDER(X, x);
        COORDINATE_ELEMENT_TYPE_DEFINITION_EXPANDER(Y, y);
        COORDINATE_ELEMENT_TYPE_DEFINITION_EXPANDER(Z, z);
        COORDINATE_ELEMENT_TYPE_DEFINITION_EXPANDER(W, w);

        // 4次元は全成分xyzwを持つ。既存の1〜3次元の軸選択とは区別する。
        struct CoordinateInfo4 {
            using Dimension = VSL_NAMESPACE::D4;
            static constexpr bool ENABLED_X = true, ENABLED_Y = true, ENABLED_Z = true, ENABLED_W = true;
            static constexpr std::size_t INDEX_X = 1, INDEX_Y = 2, INDEX_Z = 3, INDEX_W = 4;
        };

        template<bool... booleans>
        constexpr size_t count_trues_func();

        template<bool... booleans>
        struct CountTrues {
            constexpr static size_t VALUE = count_trues_func<booleans...>();
        };

        template<bool enable_x = false, bool enable_y = false, bool enable_z = false>
        struct MakeCoordinateInfo;

        template<VSL_NAMESPACE::is_dimension D, bool enable_x, bool enable_y = false, bool enable_z = false>
        struct CoordinateInfo {
            using Dimension = D;
            constexpr static bool ENABLED_X = enable_x;
            constexpr static bool ENABLED_Y = enable_y;
            constexpr static bool ENABLED_Z = enable_z;
            constexpr static std::size_t INDEX_X = count_trues_func<enable_x>();
            constexpr static std::size_t INDEX_Y = count_trues_func<enable_x, enable_y>();
            constexpr static std::size_t INDEX_Z = count_trues_func<enable_x, enable_y, enable_z>();

            consteval static typename MakeCoordinateInfo<true, enable_y, enable_z>::Value EnablingX() { return {}; };
            consteval static typename MakeCoordinateInfo<enable_x, true, enable_z>::Value EnablingY() { return {}; };
            consteval static typename MakeCoordinateInfo<enable_x, enable_y, true>::Value EnablingZ() { return {}; };

            /*
			consteval static typename MakeCoordinateInfo<false, enable_y, enable_z>::Value DisablingX() {};
			consteval static typename MakeCoordinateInfo<enable_x, false, enable_z>::Value DisablingY() {};
			consteval static typename MakeCoordinateInfo<enable_x, enable_y, false>::Value DisablingZ() {};
			*/
        };

        template<bool enable_x, bool enable_y, bool enable_z>
        struct MakeCoordinateInfo {
            using Value = CoordinateInfo<typename VSL_NAMESPACE::Dimension<CountTrues<enable_x, enable_y, enable_z>::VALUE>::Value, enable_x, enable_y, enable_z>;
        };

        template<VSL_NAMESPACE::is_dimension D>
        struct DefaultCoordinateInfo {};

        template<typename ElementType_, typename CoordinateInfo>
        struct CoordinateSet {
            static constexpr bool IS_COORDINATE_SET = true;
        };

        template<typename CI1, typename CI2>
        struct SumCoordinateInfo {
            using Value = std::conditional_t<CI1::Dimension::DIMENSION == 4 || CI2::Dimension::DIMENSION == 4,
                                             CoordinateInfo4,
                                             typename MakeCoordinateInfo<CI1::ENABLED_X || CI2::ENABLED_X, CI1::ENABLED_Y || CI2::ENABLED_Y, CI1::ENABLED_Z || CI2::ENABLED_Z>::Value>;
        };

        template<bool... booleans>
        constexpr size_t count_trues_func() {
            return (size_t{0} + ... + booleans);
        }

        template<>
        struct DefaultCoordinateInfo<VSL_NAMESPACE::D1> {
            using Value = CoordinateInfo<VSL_NAMESPACE::D1, true>;
        };

        template<>
        struct DefaultCoordinateInfo<VSL_NAMESPACE::D2> {
            using Value = CoordinateInfo<VSL_NAMESPACE::D2, true, true>;
        };

        template<>
        struct DefaultCoordinateInfo<VSL_NAMESPACE::D3> {
            using Value = CoordinateInfo<VSL_NAMESPACE::D3, true, true, true>;
        };

        template<>
        struct DefaultCoordinateInfo<VSL_NAMESPACE::D4> {
            using Value = CoordinateInfo4;
        };

#define AHO_COORDINATE_NUMBERD_MEMBER_1 ___AHO_JOIN(AHO_COORDINATE_NUMBERD_MEMBER_NAME, 1)
#define AHO_COORDINATE_NUMBERD_MEMBER_2 ___AHO_JOIN(AHO_COORDINATE_NUMBERD_MEMBER_NAME, 2)
#define AHO_COORDINATE_NUMBERD_MEMBER_3 ___AHO_JOIN(AHO_COORDINATE_NUMBERD_MEMBER_NAME, 3)

        namespace detail {
            template<std::size_t index, typename Set>
            constexpr decltype(auto) coordinate_component(Set& set) {
                using CI = typename Set::CoordinateInfo;
                static_assert(index < CI::Dimension::DIMENSION);
                if constexpr (index == 0 && CI::ENABLED_X)
                    return (set.x.value);
                else if constexpr ((index == 0 && !CI::ENABLED_X && CI::ENABLED_Y) ||
                                   (index == 1 && CI::ENABLED_X && CI::ENABLED_Y))
                    return (set.y.value);
                else if constexpr (index < 3)
                    return (set.z.value);
                else
                    return (set.w.value);
            }
        } // namespace detail

#define ___AN1 template component<0>()
#define ___AN2 template component<1>()
#define ___AN3 template component<2>()
#define ___AN4 template component<3>()

#define AHO_COORDINATE_COMPONENT_ACCESSORS                 \
    template<std::size_t index>                            \
    constexpr decltype(auto) component() {                 \
        return detail::coordinate_component<index>(*this); \
    }                                                      \
    template<std::size_t index>                            \
    constexpr decltype(auto) component() const {           \
        return detail::coordinate_component<index>(*this); \
    }

#define AHO_COORDINATE_SET_OPERATOR_ADD_ASSIGN(Upper, Lower)                               \
    template<typename T>                                                                   \
    constexpr CoordinateSet<ElementType_, CoordinateInfo>& operator+=(const Upper<T>& e) { \
        Lower += e;                                                                        \
        return *this;                                                                      \
    }                                                                                      \
    template<typename T>                                                                   \
    constexpr CoordinateSet<ElementType_, CoordinateInfo>& operator-=(const Upper<T>& e) { \
        Lower -= e;                                                                        \
        return *this;                                                                      \
    }

#define AHO_DEFINE_SPECIAL_COORDINATE_SET1(Upper, Lower)                                                 \
    template<typename ElementType_>                                                                      \
    struct CoordinateSet<ElementType_, typename MakeCoordinateInfo<                                      \
                                           std::same_as<X<ElementType_>, Upper<ElementType_>>,           \
                                           std::same_as<Y<ElementType_>, Upper<ElementType_>>,           \
                                           std::same_as<Z<ElementType_>, Upper<ElementType_>>>::Value> { \
        using ElementType = ElementType_;                                                                \
        using CoordinateInfo = typename MakeCoordinateInfo<                                              \
            std::same_as<X<ElementType_>, Upper<ElementType_>>,                                          \
            std::same_as<Y<ElementType_>, Upper<ElementType_>>,                                          \
            std::same_as<Z<ElementType_>, Upper<ElementType_>>>::Value;                                  \
        static constexpr bool IS_COORDINATE_SET = true;                                                  \
        AHO_COORDINATE_COMPONENT_ACCESSORS                                                               \
        union {                                                                                          \
            Upper<ElementType_> Lower, AHO_COORDINATE_NUMBERD_MEMBER_1;                                  \
        };                                                                                               \
        constexpr CoordinateSet() : Lower(Upper<ElementType_>()) {};                                     \
        constexpr CoordinateSet(const Upper<ElementType_>& Lower) : Lower(Lower) {};                     \
        constexpr bool operator<(const CoordinateSet<ElementType, CoordinateInfo>& o) const {            \
            return Lower < o.Lower;                                                                      \
        }                                                                                                \
        constexpr bool operator==(const CoordinateSet<ElementType, CoordinateInfo>& o) const {           \
            return Lower == o.Lower;                                                                     \
        }                                                                                                \
        constexpr auto operator<=>(const CoordinateSet<ElementType, CoordinateInfo>& o) const {          \
            return Lower <=> o.Lower;                                                                    \
        }                                                                                                \
        AHO_COORDINATE_SET_OPERATOR_ADD_ASSIGN(Upper, Lower);                                            \
    };

#define AHO_DEFINE_SPECIAL_COORDINATE_SET2(Upper1, Upper2, Lower1, Lower2)                                                                                         \
    template<typename ElementType_>                                                                                                                                \
    struct CoordinateSet<ElementType_, typename MakeCoordinateInfo<                                                                                                \
                                           (std::same_as<X<ElementType_>, Upper1<ElementType_>> || std::same_as<X<ElementType_>, Upper2<ElementType_>>),           \
                                           (std::same_as<Y<ElementType_>, Upper1<ElementType_>> || std::same_as<Y<ElementType_>, Upper2<ElementType_>>),           \
                                           (std::same_as<Z<ElementType_>, Upper1<ElementType_>> || std::same_as<Z<ElementType_>, Upper2<ElementType_>>)>::Value> { \
        using ElementType = ElementType_;                                                                                                                          \
        using CoordinateInfo = typename MakeCoordinateInfo<                                                                                                        \
            (std::same_as<X<ElementType_>, Upper1<ElementType_>> || std::same_as<X<ElementType_>, Upper2<ElementType_>>),                                          \
            (std::same_as<Y<ElementType_>, Upper1<ElementType_>> || std::same_as<Y<ElementType_>, Upper2<ElementType_>>),                                          \
            (std::same_as<Z<ElementType_>, Upper1<ElementType_>> || std::same_as<Z<ElementType_>, Upper2<ElementType_>>)>::Value;                                  \
        static constexpr bool IS_COORDINATE_SET = true;                                                                                                            \
        AHO_COORDINATE_COMPONENT_ACCESSORS                                                                                                                         \
        union {                                                                                                                                                    \
            Upper1<ElementType_> Lower1, AHO_COORDINATE_NUMBERD_MEMBER_1;                                                                                          \
        };                                                                                                                                                         \
        union {                                                                                                                                                    \
            Upper2<ElementType_> Lower2, AHO_COORDINATE_NUMBERD_MEMBER_2;                                                                                          \
        };                                                                                                                                                         \
        constexpr CoordinateSet() : Lower1(Upper1<ElementType_>()), Lower2(Upper2<ElementType_>()) {};                                                             \
        constexpr CoordinateSet(const Upper1<ElementType_>& Lower1, const Upper2<ElementType_>& Lower2) : Lower1(Lower1), Lower2(Lower2) {};                       \
        constexpr bool operator<(const CoordinateSet<ElementType, CoordinateInfo>& o) const {                                                                      \
            return (Lower1 != o.Lower1 ? Lower1 < o.Lower1 : Lower2 < o.Lower2);                                                                                   \
        }                                                                                                                                                          \
        constexpr bool operator==(const CoordinateSet<ElementType, CoordinateInfo>& o) const {                                                                     \
            return std::tie(Lower1, Lower2) == std::tie(o.Lower1, o.Lower2);                                                                                       \
        }                                                                                                                                                          \
        constexpr auto operator<=>(const CoordinateSet<ElementType, CoordinateInfo>& o) const {                                                                    \
            return std::tie(Lower1, Lower2) <=> std::tie(o.Lower1, o.Lower2);                                                                                      \
        }                                                                                                                                                          \
        AHO_COORDINATE_SET_OPERATOR_ADD_ASSIGN(Upper1, Lower1);                                                                                                    \
        AHO_COORDINATE_SET_OPERATOR_ADD_ASSIGN(Upper2, Lower2);                                                                                                    \
    };

        AHO_DEFINE_SPECIAL_COORDINATE_SET1(X, x);
        AHO_DEFINE_SPECIAL_COORDINATE_SET1(Y, y);
        AHO_DEFINE_SPECIAL_COORDINATE_SET1(Z, z);

        AHO_DEFINE_SPECIAL_COORDINATE_SET2(X, Y, x, y);
        AHO_DEFINE_SPECIAL_COORDINATE_SET2(X, Z, x, z);
        AHO_DEFINE_SPECIAL_COORDINATE_SET2(Y, Z, y, z);

        template<typename ElementType_>
        struct CoordinateSet<ElementType_, typename MakeCoordinateInfo<true, true, true>::Value> {
            using ElementType = ElementType_;
            using CoordinateInfo = MakeCoordinateInfo<true, true, true>::Value;

            static constexpr bool IS_COORDINATE_SET = true;
            AHO_COORDINATE_COMPONENT_ACCESSORS
            union {
                X<ElementType_> x, AHO_COORDINATE_NUMBERD_MEMBER_1;
            };
            union {
                Y<ElementType_> y, AHO_COORDINATE_NUMBERD_MEMBER_2;
            };
            union {
                Z<ElementType_> z, AHO_COORDINATE_NUMBERD_MEMBER_3;
            };

            constexpr CoordinateSet() : x(X<ElementType_>()), y(Y<ElementType_>()), z(Z<ElementType_>()) {}
            constexpr CoordinateSet(const X<ElementType_>& x, const Y<ElementType_>& y, const Z<ElementType_>& z) : x(x), y(y), z(z) {}
            constexpr bool operator<(const CoordinateSet<ElementType, CoordinateInfo>& o) const {
                return (x != o.x ? x < o.x : (y != o.y ? y < o.y : z < o.z));
            }
            constexpr bool operator==(const CoordinateSet<ElementType, CoordinateInfo>& o) const { return std::tie(x, y, z) == std::tie(o.x, o.y, o.z); }
            constexpr auto operator<=>(const CoordinateSet<ElementType, CoordinateInfo>& o) const { return std::tie(x, y, z) <=> std::tie(o.x, o.y, o.z); }

            AHO_COORDINATE_SET_OPERATOR_ADD_ASSIGN(X, x);
            AHO_COORDINATE_SET_OPERATOR_ADD_ASSIGN(Y, y);
            AHO_COORDINATE_SET_OPERATOR_ADD_ASSIGN(Z, z);
        };

        template<typename ElementType_>
        struct CoordinateSet<ElementType_, CoordinateInfo4> {
            using ElementType = ElementType_;
            using CoordinateInfo = CoordinateInfo4;
            static constexpr bool IS_COORDINATE_SET = true;
            X<ElementType> x{};
            Y<ElementType> y{};
            Z<ElementType> z{};
            W<ElementType> w{};
            AHO_COORDINATE_COMPONENT_ACCESSORS

            constexpr CoordinateSet() = default;
            constexpr CoordinateSet(X<ElementType> x, Y<ElementType> y, Z<ElementType> z, W<ElementType> w)
                : x(x), y(y), z(z), w(w) {}
            constexpr bool operator==(const CoordinateSet&) const = default;
            constexpr auto operator<=>(const CoordinateSet&) const = default;
            AHO_COORDINATE_SET_OPERATOR_ADD_ASSIGN(X, x);
            AHO_COORDINATE_SET_OPERATOR_ADD_ASSIGN(Y, y);
            AHO_COORDINATE_SET_OPERATOR_ADD_ASSIGN(Z, z);
            AHO_COORDINATE_SET_OPERATOR_ADD_ASSIGN(W, w);
        };

        template<typename T1, typename CI1, typename T2, typename CI2>
        constexpr CoordinateSet<decltype(std::declval<T1>() + std::declval<T2>()), typename SumCoordinateInfo<CI1, CI2>::Value> operator+(const CoordinateSet<T1, CI1>& e1, const CoordinateSet<T2, CI2>& e2) {
            CoordinateSet<decltype(std::declval<T1>() + std::declval<T2>()), typename SumCoordinateInfo<CI1, CI2>::Value> result;
            if constexpr (CI1::ENABLED_X) {
                result += e1.x;
            }
            if constexpr (CI1::ENABLED_Y) {
                result += e1.y;
            }
            if constexpr (CI1::ENABLED_Z) {
                result += e1.z;
            }
            if constexpr (CI1::Dimension::DIMENSION == 4)
                result += e1.w;

            if constexpr (CI2::ENABLED_X) {
                result += e2.x;
            }
            if constexpr (CI2::ENABLED_Y) {
                result += e2.y;
            }
            if constexpr (CI2::ENABLED_Z) {
                result += e2.z;
            }
            if constexpr (CI2::Dimension::DIMENSION == 4)
                result += e2.w;

            return result;
        }

        template<typename T1, typename CI1, typename T2>
        constexpr CoordinateSet<decltype(std::declval<T1>() + std::declval<T2>()), typename SumCoordinateInfo<CI1, MakeCoordinateInfo<true, false, false>::Value>::Value> operator+(const CoordinateSet<T1, CI1>& e1, const X<T2>& e2) {
            CoordinateSet<decltype(std::declval<T1>() + std::declval<T2>()), typename SumCoordinateInfo<CI1, MakeCoordinateInfo<true, false, false>::Value>::Value> result;
            if constexpr (CI1::ENABLED_X) {
                result += e1.x;
            }
            if constexpr (CI1::ENABLED_Y) {
                result += e1.y;
            }
            if constexpr (CI1::ENABLED_Z) {
                result += e1.z;
            }
            if constexpr (CI1::Dimension::DIMENSION == 4)
                result += e1.w;

            result += e2;

            return result;
        }

        template<typename T1, typename CI1, typename T2>
        constexpr CoordinateSet<decltype(std::declval<T1>() + std::declval<T2>()), typename SumCoordinateInfo<CI1, MakeCoordinateInfo<false, true, false>::Value>::Value> operator+(const CoordinateSet<T1, CI1>& e1, const Y<T2>& e2) {
            CoordinateSet<decltype(std::declval<T1>() + std::declval<T2>()), typename SumCoordinateInfo<CI1, MakeCoordinateInfo<false, true, false>::Value>::Value> result;
            if constexpr (CI1::ENABLED_X) {
                result += e1.x;
            }
            if constexpr (CI1::ENABLED_Y) {
                result += e1.y;
            }
            if constexpr (CI1::ENABLED_Z) {
                result += e1.z;
            }
            if constexpr (CI1::Dimension::DIMENSION == 4)
                result += e1.w;

            result += e2;

            return result;
        }

        template<typename T1, typename CI1, typename T2>
        constexpr CoordinateSet<decltype(std::declval<T1>() + std::declval<T2>()), typename SumCoordinateInfo<CI1, MakeCoordinateInfo<false, false, true>::Value>::Value> operator+(const CoordinateSet<T1, CI1>& e1, const Z<T2>& e2) {
            CoordinateSet<decltype(std::declval<T1>() + std::declval<T2>()), typename SumCoordinateInfo<CI1, MakeCoordinateInfo<false, false, true>::Value>::Value> result;
            if constexpr (CI1::ENABLED_X) {
                result += e1.x;
            }
            if constexpr (CI1::ENABLED_Y) {
                result += e1.y;
            }
            if constexpr (CI1::ENABLED_Z) {
                result += e1.z;
            }
            if constexpr (CI1::Dimension::DIMENSION == 4)
                result += e1.w;

            result += e2;

            return result;
        }

        template<typename T1, typename T2>
        constexpr CoordinateSet<decltype(std::declval<T1>() + std::declval<T2>()), typename MakeCoordinateInfo<true, true, false>::Value> operator+(const X<T1>& e1, const Y<T2>& e2) {
            using Result = decltype(std::declval<T1>() + std::declval<T2>());
            return {static_cast<Result>(e1.value), static_cast<Result>(e2.value)};
        }

        template<typename T1, typename T2>
        constexpr CoordinateSet<decltype(std::declval<T1>() + std::declval<T2>()), typename MakeCoordinateInfo<true, false, true>::Value> operator+(const X<T1>& e1, const Z<T2>& e2) {
            using Result = decltype(std::declval<T1>() + std::declval<T2>());
            return {static_cast<Result>(e1.value), static_cast<Result>(e2.value)};
        }

        template<typename T1, typename T2>
        constexpr CoordinateSet<decltype(std::declval<T1>() + std::declval<T2>()), typename MakeCoordinateInfo<false, true, true>::Value> operator+(const Y<T1>& e1, const Z<T2>& e2) {
            using Result = decltype(std::declval<T1>() + std::declval<T2>());
            return {static_cast<Result>(e1.value), static_cast<Result>(e2.value)};
        }

        template<typename T1, typename CI1, typename T2, typename CI2>
        constexpr CoordinateSet<decltype(std::declval<T1>() - std::declval<T2>()), typename SumCoordinateInfo<CI1, CI2>::Value> operator-(const CoordinateSet<T1, CI1>& e1, const CoordinateSet<T2, CI2>& e2) {
            CoordinateSet<decltype(std::declval<T1>() - std::declval<T2>()), typename SumCoordinateInfo<CI1, CI2>::Value> result;
            if constexpr (CI1::ENABLED_X) {
                result += e1.x;
            }
            if constexpr (CI1::ENABLED_Y) {
                result += e1.y;
            }
            if constexpr (CI1::ENABLED_Z) {
                result += e1.z;
            }
            if constexpr (CI1::Dimension::DIMENSION == 4)
                result += e1.w;

            if constexpr (CI2::ENABLED_X) {
                result -= e2.x;
            }
            if constexpr (CI2::ENABLED_Y) {
                result -= e2.y;
            }
            if constexpr (CI2::ENABLED_Z) {
                result -= e2.z;
            }
            if constexpr (CI2::Dimension::DIMENSION == 4)
                result -= e2.w;

            return result;
        }

        template<typename T1, typename CI1, typename T2>
        constexpr CoordinateSet<decltype(std::declval<T1>() - std::declval<T2>()), typename SumCoordinateInfo<CI1, MakeCoordinateInfo<true, false, false>::Value>::Value> operator-(const CoordinateSet<T1, CI1>& e1, const X<T2>& e2) {
            CoordinateSet<decltype(std::declval<T1>() - std::declval<T2>()), typename SumCoordinateInfo<CI1, MakeCoordinateInfo<true, false, false>::Value>::Value> result;
            if constexpr (CI1::ENABLED_X) {
                result += e1.x;
            }
            if constexpr (CI1::ENABLED_Y) {
                result += e1.y;
            }
            if constexpr (CI1::ENABLED_Z) {
                result += e1.z;
            }
            if constexpr (CI1::Dimension::DIMENSION == 4)
                result += e1.w;

            result -= e2;

            return result;
        }

        template<typename T1, typename CI1, typename T2>
        constexpr CoordinateSet<decltype(std::declval<T1>() - std::declval<T2>()), typename SumCoordinateInfo<CI1, MakeCoordinateInfo<false, true, false>::Value>::Value> operator-(const CoordinateSet<T1, CI1>& e1, const Y<T2>& e2) {
            CoordinateSet<decltype(std::declval<T1>() - std::declval<T2>()), typename SumCoordinateInfo<CI1, MakeCoordinateInfo<false, true, false>::Value>::Value> result;
            if constexpr (CI1::ENABLED_X) {
                result += e1.x;
            }
            if constexpr (CI1::ENABLED_Y) {
                result += e1.y;
            }
            if constexpr (CI1::ENABLED_Z) {
                result += e1.z;
            }
            if constexpr (CI1::Dimension::DIMENSION == 4)
                result += e1.w;

            result -= e2;

            return result;
        }

        template<typename T1, typename CI1, typename T2>
        constexpr CoordinateSet<decltype(std::declval<T1>() - std::declval<T2>()), typename SumCoordinateInfo<CI1, MakeCoordinateInfo<false, false, true>::Value>::Value> operator-(const CoordinateSet<T1, CI1>& e1, const Z<T2>& e2) {
            CoordinateSet<decltype(std::declval<T1>() - std::declval<T2>()), typename SumCoordinateInfo<CI1, MakeCoordinateInfo<false, false, true>::Value>::Value> result;
            if constexpr (CI1::ENABLED_X) {
                result += e1.x;
            }
            if constexpr (CI1::ENABLED_Y) {
                result += e1.y;
            }
            if constexpr (CI1::ENABLED_Z) {
                result += e1.z;
            }
            if constexpr (CI1::Dimension::DIMENSION == 4)
                result += e1.w;

            result -= e2;

            return result;
        }

#define AHO_DEFINE_COORDINATE_SET_SUB_COORDINATE_THAN(U1, U2, EnableX, EnableY, EnableZ)                                                                                                      \
    template<typename T1, typename T2>                                                                                                                                                        \
    constexpr CoordinateSet<decltype(std::declval<T1>() - std::declval<T2>()), typename MakeCoordinateInfo<EnableX, EnableY, EnableZ>::Value> operator-(const U1<T1>& e1, const U2<T2>& e2) { \
        using Result = decltype(std::declval<T1>() - std::declval<T2>());                                                                                                                     \
        return {static_cast<Result>(e1.value), -static_cast<Result>(e2.value)};                                                                                                               \
    }

#define AHO_DEFINE_COORDINATE_SET_SUB_COORDINATE_LESS(U1, U2, EnableX, EnableY, EnableZ)                                                                                                      \
    template<typename T1, typename T2>                                                                                                                                                        \
    constexpr CoordinateSet<decltype(std::declval<T1>() - std::declval<T2>()), typename MakeCoordinateInfo<EnableX, EnableY, EnableZ>::Value> operator-(const U1<T1>& e1, const U2<T2>& e2) { \
        using Result = decltype(std::declval<T1>() - std::declval<T2>());                                                                                                                     \
        return {-static_cast<Result>(e2.value), static_cast<Result>(e1.value)};                                                                                                               \
    }

        AHO_DEFINE_COORDINATE_SET_SUB_COORDINATE_THAN(X, Y, true, true, false)
        AHO_DEFINE_COORDINATE_SET_SUB_COORDINATE_THAN(X, Z, true, false, true)
        AHO_DEFINE_COORDINATE_SET_SUB_COORDINATE_THAN(Y, Z, false, true, true)

        AHO_DEFINE_COORDINATE_SET_SUB_COORDINATE_LESS(Z, X, true, false, true)
        AHO_DEFINE_COORDINATE_SET_SUB_COORDINATE_LESS(Y, X, true, true, false)
        AHO_DEFINE_COORDINATE_SET_SUB_COORDINATE_LESS(Z, Y, false, true, true)

        namespace literals {
            constexpr X<double> operator AHO_LITERAL(x)(long double v) {
                return {(double)v};
            };
            constexpr X<int> operator AHO_LITERAL(x)(unsigned long long v) {
                return {(int)v};
            };
            constexpr X<float> operator AHO_LITERAL(f_x)(long double v) {
                return {(float)v};
            };
            constexpr X<size_t> operator AHO_LITERAL(l_x)(unsigned long long v) {
                return {v};
            };
            constexpr X<double> X_(1.0);

            constexpr Y<double> operator AHO_LITERAL(y)(long double v) {
                return {(double)v};
            };
            constexpr Y<int> operator AHO_LITERAL(y)(unsigned long long v) {
                return {(int)v};
            };
            constexpr Y<float> operator AHO_LITERAL(f_y)(long double v) {
                return {(float)v};
            };
            constexpr Y<size_t> operator AHO_LITERAL(l_y)(unsigned long long v) {
                return {v};
            };
            constexpr Y<double> Y_(1.0);

            constexpr Z<double> operator AHO_LITERAL(z)(long double v) {
                return {(double)v};
            };
            constexpr Z<int> operator AHO_LITERAL(z)(unsigned long long v) {
                return {(int)v};
            };
            constexpr Z<float> operator AHO_LITERAL(f_z)(long double v) {
                return {(float)v};
            };
            constexpr Z<size_t> operator AHO_LITERAL(l_z)(unsigned long long v) {
                return {v};
            };
            constexpr Z<double> Z_(1.0);
        } // namespace literals
    } // namespace coordinate

    namespace concepts {
        template<typename R>
        concept IS_COORDINATE_SET = requires {
            typename std::enable_if<R::IS_COORDINATE_SET>::type;
        };
    } // namespace concepts

    namespace literals {
        using namespace coordinate::literals;
    }

    template<typename ElementType_, typename CoordinationInfo>
    std::ostream& operator<<(std::ostream& out, const coordinate::CoordinateSet<ElementType_, CoordinationInfo>& set) {
        if constexpr (std::same_as<typename CoordinationInfo::Dimension, VSL_NAMESPACE::D1>) {
            out << set.AHO_COORDINATE_NUMBERD_MEMBER_1;
        }

        if constexpr (std::same_as<typename CoordinationInfo::Dimension, VSL_NAMESPACE::D2>) {
            out << "(" << decltype(set.AHO_COORDINATE_NUMBERD_MEMBER_1)::LOWER_CHAR
                << ", " << decltype(set.AHO_COORDINATE_NUMBERD_MEMBER_2)::LOWER_CHAR << ")"
                << "="
                << "(" << set.AHO_COORDINATE_NUMBERD_MEMBER_1.value
                << ", " << set.AHO_COORDINATE_NUMBERD_MEMBER_2.value << ")";
        }

        if constexpr (std::same_as<typename CoordinationInfo::Dimension, VSL_NAMESPACE::D3>) {
            out << "(" << decltype(set.AHO_COORDINATE_NUMBERD_MEMBER_1)::LOWER_CHAR
                << ", " << decltype(set.AHO_COORDINATE_NUMBERD_MEMBER_2)::LOWER_CHAR
                << ", " << decltype(set.AHO_COORDINATE_NUMBERD_MEMBER_3)::LOWER_CHAR << ")"
                << "="
                << "(" << set.AHO_COORDINATE_NUMBERD_MEMBER_1.value
                << ", " << set.AHO_COORDINATE_NUMBERD_MEMBER_2.value
                << ", " << set.AHO_COORDINATE_NUMBERD_MEMBER_3.value << ")";
        }
        if constexpr (std::same_as<typename CoordinationInfo::Dimension, VSL_NAMESPACE::D4>)
            out << "(x, y, z, w)=(" << set.___AN1 << ", " << set.___AN2 << ", " << set.___AN3 << ", " << set.___AN4 << ")";
        return out;
    };

    using coordinate::X;
    using coordinate::Y;
    using coordinate::Z;
    using coordinate::W;
} // namespace AHO_NAMESPACE
