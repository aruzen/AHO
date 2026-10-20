#pragma once
#include <compare>

namespace aho_test::arithmetic {
    struct Operand {
        int value{};
        auto operator<=>(const Operand&) const = default;
    };
    struct LeftOperand {
        int value{};
        auto operator<=>(const LeftOperand&) const = default;
        constexpr LeftOperand& operator+=(const Operand& rhs) {
            value += rhs.value;
            return *this;
        }
        constexpr LeftOperand& operator-=(const Operand& rhs) {
            value -= rhs.value;
            return *this;
        }
        constexpr LeftOperand& operator*=(const Operand& rhs) {
            value *= rhs.value;
            return *this;
        }
        constexpr LeftOperand& operator/=(const Operand& rhs) {
            value /= rhs.value;
            return *this;
        }
    };
    constexpr int operator+(const LeftOperand& lhs, const Operand& rhs) { return lhs.value + rhs.value; }
    constexpr int operator-(const LeftOperand& lhs, const Operand& rhs) { return lhs.value - rhs.value; }
    constexpr int operator*(const LeftOperand& lhs, const Operand& rhs) { return lhs.value * rhs.value; }
    constexpr int operator/(const LeftOperand& lhs, const Operand& rhs) { return lhs.value / rhs.value; }

    struct AssignmentOnly {
        int value{};
        auto operator<=>(const AssignmentOnly&) const = default;
        constexpr AssignmentOnly& operator+=(const Operand& rhs) {
            value += rhs.value;
            return *this;
        }
        constexpr AssignmentOnly& operator-=(const Operand& rhs) {
            value -= rhs.value;
            return *this;
        }
        constexpr AssignmentOnly& operator*=(const Operand& rhs) {
            value *= rhs.value;
            return *this;
        }
        constexpr AssignmentOnly& operator/=(const Operand& rhs) {
            value /= rhs.value;
            return *this;
        }
    };
    struct CategoryResult {
        int value{};
        auto operator<=>(const CategoryResult&) const = default;
    };
    constexpr int operator+(const CategoryResult& lhs, const CategoryResult& rhs) { return lhs.value + rhs.value; }
    constexpr long operator+(CategoryResult&& lhs, CategoryResult&& rhs) { return static_cast<long>(lhs.value + rhs.value); }
    constexpr int operator-(const CategoryResult& lhs, const CategoryResult& rhs) { return lhs.value - rhs.value; }
    constexpr long operator-(CategoryResult&& lhs, CategoryResult&& rhs) { return static_cast<long>(lhs.value - rhs.value); }
    constexpr int operator*(const CategoryResult& lhs, const CategoryResult& rhs) { return lhs.value * rhs.value; }
    constexpr long operator*(CategoryResult&& lhs, CategoryResult&& rhs) { return static_cast<long>(lhs.value * rhs.value); }
    constexpr int operator/(const CategoryResult& lhs, const CategoryResult& rhs) { return lhs.value / rhs.value; }
    constexpr long operator/(CategoryResult&& lhs, CategoryResult&& rhs) { return static_cast<long>(lhs.value / rhs.value); }

    constexpr long operator*(CategoryResult&& lhs, const CategoryResult& rhs) { return static_cast<long>(lhs.value * rhs.value); }
    constexpr long operator/(CategoryResult&& lhs, const CategoryResult& rhs) { return static_cast<long>(lhs.value / rhs.value); }

    struct MutableOnly {
        int value{};
        auto operator<=>(const MutableOnly&) const = default;
        constexpr int operator+(const MutableOnly& rhs) { return value + rhs.value; }
        constexpr int operator-(const MutableOnly& rhs) { return value - rhs.value; }
        constexpr int operator*(const MutableOnly& rhs) { return value * rhs.value; }
        constexpr int operator/(const MutableOnly& rhs) { return value / rhs.value; }
    };

    template<class L, class R>
    concept CanAdd = requires(const L& lhs, const R& rhs) { lhs + rhs; };
    template<class L, class R>
    concept CanAddAssign = requires(L& lhs, const R& rhs) { lhs += rhs; };
    template<class L, class R>
    concept CanSubtract = requires(const L& lhs, const R& rhs) { lhs - rhs; };
    template<class L, class R>
    concept CanSubtractAssign = requires(L& lhs, const R& rhs) { lhs -= rhs; };
    template<class L, class R>
    concept CanMultiply = requires(const L& lhs, const R& rhs) { lhs * rhs; };
    template<class L, class R>
    concept CanMultiplyAssign = requires(L& lhs, const R& rhs) { lhs *= rhs; };
    template<class L, class R>
    concept CanDivide = requires(const L& lhs, const R& rhs) { lhs / rhs; };
    template<class L, class R>
    concept CanDivideAssign = requires(L& lhs, const R& rhs) { lhs /= rhs; };

    template<template<class> class Wrapper>
    struct Family {
        template<class T>
        using Of = Wrapper<T>;
    };
} // namespace aho_test::arithmetic
