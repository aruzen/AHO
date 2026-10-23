#pragma once

#include "../../define.hpp"

#include <array>
#include <algorithm>
#include <optional>
#include <expected>
#include <limits>
#include <type_traits>
#include <cmath>

#include "../../concepts.hpp"
#include "VSL/dimension.hpp"

#include "../ov.hpp"
#include "angle.hpp"
#include "coordinate.hpp"

namespace AHO_NAMESPACE {
    namespace matrix {
        template<typename R, size_t row, size_t column>
        struct Mat;

        struct Type {
            size_t row, column;
        };

        enum class FailedCause {
            NOT_SQUARE,
            SINGULAR,
            NON_AFFINE
        };

        template<typename T>
        concept to_matrixable = requires(T& t) {
            typename T::ElementType;
            { t.Type_() } -> std::same_as<matrix::Type>;
            { t.to_matrix() } -> std::same_as<Mat<typename T::ElementType, T::Type_().row, T::Type_().column>>;
        };

        template<typename T>
        concept is_matrix = requires(T& t) {
            { to_matrixable<T> };
            t + std::declval<Mat<typename T::ElementType, T::Type_().row, T::Type_().column>>();
            t - std::declval<Mat<typename T::ElementType, T::Type_().row, T::Type_().column>>();
            t * std::declval<Mat<typename T::ElementType, T::Type_().row, T::Type_().column>>();
            t += std::declval<Mat<typename T::ElementType, T::Type_().row, T::Type_().column>>();
            t -= std::declval<Mat<typename T::ElementType, T::Type_().row, T::Type_().column>>();
            t *= std::declval<Mat<typename T::ElementType, T::Type_().row, T::Type_().column>>();
        };

        namespace detail {
            template<typename L, typename R>
            using ProductType = decltype(std::declval<const L&>() * std::declval<const R&>());

            template<typename L, typename R, size_t rows, size_t columns>
            concept matrix_product = concepts::multiply_as<const L&, const R&> &&
                                     std::is_object_v<ProductType<L, R>> &&
                                     requires(const L& lhs, const R& rhs, ProductType<L, R>& total) { total += lhs * rhs; } &&
                                     (rows == 0 || columns == 0 || std::default_initializable<ProductType<L, R>>);
        } // namespace detail

        template<typename R, size_t row, size_t column>
        struct Mat : public std::array<std::array<R, column>, row> {
            static constexpr bool IS_MAT = true;

            using ElementType = R;

            using RowType = std::array<R, column>;
            using OriginArray = std::array<RowType, row>;

            static consteval Type Type_() {
                return {row, column};
            }

            constexpr Mat() = default;

            constexpr Mat(const std::array<std::array<R, column>, row>& init);

            constexpr Mat(const std::initializer_list<std::initializer_list<R>>& init);

            constexpr AHO_NAMESPACE::matrix::Mat<R, row, column> operator+() const;

            constexpr AHO_NAMESPACE::matrix::Mat<R, row, column> operator-() const;

            template<typename T>
            requires concepts::add_as<const R&, const T&>
            constexpr auto operator+(const AHO_NAMESPACE::matrix::Mat<T, row, column>& p) const;

            template<typename T>
            requires concepts::subtract_as<const R&, const T&>
            constexpr auto operator-(const AHO_NAMESPACE::matrix::Mat<T, row, column>& p) const;

            template<typename T, size_t target_column>
            requires detail::matrix_product<R, T, row, target_column>
            constexpr auto operator*(const AHO_NAMESPACE::matrix::Mat<T, column, target_column>& p) const;

            template<typename T>
            requires concepts::add_assign_as<R&, const T&>
            constexpr Mat<R, row, column>& operator+=(const AHO_NAMESPACE::matrix::Mat<T, row, column>& p);

            template<typename T>
            requires concepts::subtract_assign_as<R&, const T&>
            constexpr Mat<R, row, column>& operator-=(const AHO_NAMESPACE::matrix::Mat<T, row, column>& p);

            template<typename T>
            requires(row == column) && detail::matrix_product<R, T, row, column> &&
                    requires(R& target, const detail::ProductType<R, T>& value) { target = value; }
            constexpr Mat<R, row, column>& operator*=(const AHO_NAMESPACE::matrix::Mat<T, row, column>& p);

            template<typename T>
            requires concepts::multiply_assign_as<R&, const T&>
            constexpr Mat<R, row, column>& operator*=(const T& m);

            template<typename T>
            requires concepts::division_assign_as<R&, const T&>
            constexpr Mat<R, row, column>& operator/=(const T& m);

            template<typename T>
            requires AHO_NAMESPACE::concepts::cast_as<R, T>
            Mat<T, row, column> cast() const;
        };

        template<typename L, size_t row, size_t column, typename R>
        requires(!requires {{ R::IS_MAT } -> std::convertible_to<bool>; }) && concepts::multiply_as<const L&, const R&>
        constexpr auto operator*(const matrix::Mat<L, row, column>& mat, const R& m);

        template<typename L, size_t row, size_t column, typename R>
        requires(!requires {{ R::IS_MAT } -> std::convertible_to<bool>; }) && concepts::division_as<const L&, const R&>
        constexpr auto operator/(const matrix::Mat<L, row, column>& mat, const R& m);

        template<typename L, size_t row, size_t column, typename R>
        requires(!requires {{ R::IS_MAT } -> std::convertible_to<bool>; }) && concepts::multiply_as<const R&, const L&>
        constexpr auto operator*(const R& m, const matrix::Mat<L, row, column>& mat);

        template<typename L, size_t row, size_t column, typename R>
        requires(!requires {{ R::IS_MAT } -> std::convertible_to<bool>; }) && concepts::division_as<const R&, const L&>
        constexpr auto operator/(const R& m, const matrix::Mat<L, row, column>& mat);
    } // namespace matrix

    template<typename R, size_t row, size_t column>
    using Mat = matrix::Mat<R, row, column>;

    template<typename R>
    using Mat2x2 = matrix::Mat<R, 2, 2>;

    template<typename R>
    using Mat4x4 = matrix::Mat<R, 4, 4>;

    using Mat2x2F = matrix::Mat<float, 2, 2>;

    using Mat4x4F = matrix::Mat<float, 4, 4>;

    //------------------------------------------------------------------------------------------

    namespace matrix {
        template<typename R, size_t row, size_t column>
        constexpr Mat<R, row, column>::Mat(const std::array<std::array<R, column>, row>& init)
            : std::array<std::array<R, column>, row>{init} {}

        template<typename R, size_t row_, size_t column_>
        constexpr Mat<R, row_, column_>::Mat(const std::initializer_list<std::initializer_list<R>>& init)
            : OriginArray{} {
            size_t row = 0;
            for (const auto& source_row : init) {
                if (row == row_)
                    break;
                size_t column = 0;
                for (const auto& value : source_row) {
                    if (column == column_)
                        break;
                    (*this)[row][column++] = value;
                }
                ++row;
            }
        }

        template<typename R, size_t row, size_t column>
        constexpr AHO_NAMESPACE::matrix::Mat<R, row, column> Mat<R, row, column>::operator+() const {
            return *this;
        }

        template<typename R, size_t row, size_t column>
        constexpr AHO_NAMESPACE::matrix::Mat<R, row, column> Mat<R, row, column>::operator-() const {
            Mat<R, row, column> buf;
            for (size_t i = 0; i < row; i++)
                for (size_t j = 0; j < column; j++)
                    buf[i][j] = -(*this)[i][j];
            return std::move(buf);
        }

        template<typename R, size_t row, size_t column>
        template<typename T>
        requires concepts::add_as<const R&, const T&>
        constexpr auto Mat<R, row, column>::operator+(const Mat<T, row, column>& p) const {
            Mat<decltype((*this)[0][0] + p[0][0]), row, column> buf;
            for (size_t i = 0; i < row; i++)
                for (size_t j = 0; j < column; j++)
                    buf[i][j] = (*this)[i][j] + p[i][j];
            return std::move(buf);
        }

        template<typename R, size_t row, size_t column>
        template<typename T>
        requires concepts::subtract_as<const R&, const T&>
        constexpr auto Mat<R, row, column>::operator-(const Mat<T, row, column>& p) const {
            Mat<decltype((*this)[0][0] - p[0][0]), row, column> buf;
            for (size_t i = 0; i < row; i++)
                for (size_t j = 0; j < column; j++)
                    buf[i][j] = (*this)[i][j] - p[i][j];
            return std::move(buf);
        }

        template<typename R, size_t row, size_t column>
        template<typename T, size_t target_column>
        requires detail::matrix_product<R, T, row, target_column>
        constexpr auto
        Mat<R, row, column>::operator*(const AHO_NAMESPACE::matrix::Mat<T, column, target_column>& p) const {
            AHO_NAMESPACE::matrix::Mat<decltype((*this)[0][0] * p[0][0]), row, target_column> buf{};

            for (size_t i = 0; i < row; i++)
                for (size_t j = 0; j < target_column; j++)
                    for (size_t k = 0; k < column; k++)
                        buf[i][j] += (*this)[i][k] * p[k][j];
            return std::move(buf);
        }

        template<typename R, size_t row, size_t column>
        template<typename T>
        requires concepts::add_assign_as<R&, const T&>
        constexpr Mat<R, row, column>&
        Mat<R, row, column>::operator+=(const AHO_NAMESPACE::matrix::Mat<T, row, column>& p) {
            for (size_t i = 0; i < row; i++)
                for (size_t j = 0; j < column; j++)
                    (*this)[i][j] += p[i][j];
            return *this;
        }

        template<typename R, size_t row, size_t column>
        template<typename T>
        requires concepts::subtract_assign_as<R&, const T&>
        constexpr Mat<R, row, column>&
        Mat<R, row, column>::operator-=(const AHO_NAMESPACE::matrix::Mat<T, row, column>& p) {
            for (size_t i = 0; i < row; i++)
                for (size_t j = 0; j < column; j++)
                    (*this)[i][j] -= p[i][j];
            return *this;
        }

        template<typename R, size_t row, size_t column>
        template<typename T>
        requires(row == column) && detail::matrix_product<R, T, row, column> &&
                requires(R& target, const detail::ProductType<R, T>& value) { target = value; }
        constexpr Mat<R, row, column>&
        Mat<R, row, column>::operator*=(const AHO_NAMESPACE::matrix::Mat<T, row, column>& p) {
            const auto product = *this * p;
            for (size_t i = 0; i < row; i++)
                for (size_t j = 0; j < column; j++)
                    (*this)[i][j] = product[i][j];
            return *this;
        }

        template<typename R, size_t row, size_t column>
        template<typename T>
        requires concepts::multiply_assign_as<R&, const T&>
        constexpr Mat<R, row, column>& Mat<R, row, column>::operator*=(const T& m) {
            for (size_t i = 0; i < row; i++)
                for (size_t j = 0; j < column; j++)
                    (*this)[i][j] *= m;
            return *this;
        }

        template<typename R, size_t row, size_t column>
        template<typename T>
        requires concepts::division_assign_as<R&, const T&>
        constexpr Mat<R, row, column>& Mat<R, row, column>::operator/=(const T& m) {
            for (size_t i = 0; i < row; i++)
                for (size_t j = 0; j < column; j++)
                    (*this)[i][j] /= m;
            return *this;
        }

        template<typename R, size_t row, size_t column>
        template<typename T>
        requires AHO_NAMESPACE::concepts::cast_as<R, T>
        Mat<T, row, column> Mat<R, row, column>::cast() const {
            AHO_NAMESPACE::matrix::Mat<T, row, column> buf;
            for (size_t i = 0; i < row; i++)
                for (size_t j = 0; j < column; j++)
                    buf[i][j] = (T)(*this)[i][j];
            return std::move(buf);
        }

        template<typename R, size_t row, size_t column, typename T>
        requires(!requires {{ T::IS_MAT } -> std::convertible_to<bool>; }) && concepts::multiply_as<const R&, const T&>
        constexpr auto operator*(const Mat<R, row, column>& mat, const T& m) {
            AHO_NAMESPACE::matrix::Mat<decltype(mat[0][0] * m), row, column> buf;
            for (size_t i = 0; i < row; i++)
                for (size_t j = 0; j < column; j++)
                    buf[i][j] = mat[i][j] * m;
            return std::move(buf);
        }

        template<typename R, size_t row, size_t column, typename T>
        requires(!requires {{ T::IS_MAT } -> std::convertible_to<bool>; }) && concepts::division_as<const R&, const T&>
        constexpr auto operator/(const Mat<R, row, column>& mat, const T& m) {
            AHO_NAMESPACE::matrix::Mat<decltype(mat[0][0] / m), row, column> buf;

            for (size_t i = 0; i < row; i++)
                for (size_t j = 0; j < column; j++)
                    buf[i][j] = mat[i][j] / m;
            return std::move(buf);
        }

        template<typename R, size_t row, size_t column, typename T>
        requires(!requires {{ T::IS_MAT } -> std::convertible_to<bool>; }) && concepts::multiply_as<const T&, const R&>
        constexpr auto operator*(const T& m, const Mat<R, row, column>& mat) {
            AHO_NAMESPACE::matrix::Mat<decltype(m * mat[0][0]), row, column> buf;

            for (size_t i = 0; i < row; i++)
                for (size_t j = 0; j < column; j++)
                    buf[i][j] = m * mat[i][j];
            return std::move(buf);
        }

        template<typename R, size_t row, size_t column, typename T>
        requires(!requires {{ T::IS_MAT } -> std::convertible_to<bool>; }) && concepts::division_as<const T&, const R&>
        constexpr auto operator/(const T& m, const Mat<R, row, column>& mat) {
            AHO_NAMESPACE::matrix::Mat<decltype(m / mat[0][0]), row, column> buf;

            for (size_t i = 0; i < row; i++)
                for (size_t j = 0; j < column; j++)
                    buf[i][j] = m / mat[i][j];
            return std::move(buf);
        }

        template<typename R, size_t row, size_t column>
        std::ostream& operator<<(std::ostream& out, const Mat<R, row, column>& mat) {
            out << "{";
            for (size_t i = 0; i < row; i++) {
                out << "{";
                for (size_t j = 0; j < column; j++) {
                    if (j != 0)
                        out << ", ";
                    out << mat[i][j];
                }
                out << (i + 1 == row ? "}}" : "}") << std::endl;
            }
            if constexpr (row == 0)
                out << "}";
            return out;
        };
    } // namespace matrix
} // namespace AHO_NAMESPACE
