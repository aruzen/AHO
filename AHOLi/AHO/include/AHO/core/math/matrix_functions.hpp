#pragma once
#include "mat.hpp"
#include "conventions.hpp"

namespace AHO_NAMESPACE::matrix {
    template<typename T, size_t n1>
    std::array<T, n1> row(const T (&arr)[n1]);

    template<typename T, size_t row, size_t column>
    constexpr Mat<T, row, column> make(const std::array<T, column> (&arr)[row]);

    template<typename T, size_t row, size_t column, MatrixLayout layout = DEFAULT_MATRIX_LAYOUT>
    constexpr Mat<T, row, column> make(std::initializer_list<std::initializer_list<T>>);

    template<typename R, size_t size>
    Mat<R, size, size> make_identity();

    template<matrix::is_matrix R>
    requires(R::Type_().row == R::Type_().column)
    Mat<typename R::ElementType, R::Type_().row, R::Type_().column> make_identity();

    template<typename R, size_t row, size_t column>
    std::expected<Mat<R, row, column>, FailedCause> inverse_gauss_jordan(const Mat<R, row, column>& mat);

    template<typename R>
    std::expected<Mat<R, 4, 4>, FailedCause> inverse_affine(const Mat<R, 4, 4>& mat);

    template<typename T, std::size_t rows, std::size_t columns>
    constexpr Mat<T, columns, rows> transpose(const Mat<T, rows, columns>& value);

    template<MatrixLayout layout = DEFAULT_MATRIX_LAYOUT, typename T, std::size_t rows, std::size_t columns>
    constexpr std::array<T, rows * columns> to_array(const Mat<T, rows, columns>& value);

    template<typename T, std::size_t rows, std::size_t columns, MatrixLayout layout = DEFAULT_MATRIX_LAYOUT>
    constexpr Mat<T, rows, columns> from_array(const std::array<T, rows * columns>& value);

    namespace detail {
        template<VectorConvention convention, typename T, std::size_t size>
        constexpr Mat<T, size, size> with_vector_convention(const Mat<T, size, size>& column_matrix);
    } // namespace detail

    template<typename T, std::size_t rows, std::size_t columns>
    constexpr Mat<T, columns, rows> transpose(const Mat<T, rows, columns>& value) {
        Mat<T, columns, rows> result{};
        for (std::size_t r = 0; r < rows; ++r)
            for (std::size_t c = 0; c < columns; ++c)
                result[c][r] = value[r][c];
        return result;
    }

    template<MatrixLayout layout, typename T, std::size_t rows, std::size_t columns>
    constexpr std::array<T, rows * columns> to_array(const Mat<T, rows, columns>& value) {
        std::array<T, rows * columns> result{};
        for (std::size_t r = 0; r < rows; ++r)
            for (std::size_t c = 0; c < columns; ++c) {
                if constexpr (layout == MatrixLayout::COLUMN_MAJOR)
                    result[c * rows + r] = value[r][c];
                else
                    result[r * columns + c] = value[r][c];
            }
        return result;
    }

    template<typename T, std::size_t rows, std::size_t columns, MatrixLayout layout>
    constexpr Mat<T, rows, columns> from_array(const std::array<T, rows * columns>& value) {
        Mat<T, rows, columns> result{};
        for (std::size_t r = 0; r < rows; ++r)
            for (std::size_t c = 0; c < columns; ++c) {
                if constexpr (layout == MatrixLayout::COLUMN_MAJOR)
                    result[r][c] = value[c * rows + r];
                else
                    result[r][c] = value[r * columns + c];
            }
        return result;
    }

    namespace detail {
        template<VectorConvention convention, typename T, std::size_t size>
        constexpr Mat<T, size, size> with_vector_convention(const Mat<T, size, size>& column_matrix) {
            if constexpr (convention == VectorConvention::ROW)
                return transpose(column_matrix);
            else
                return column_matrix;
        }
    } // namespace detail

    template<typename T, size_t n1>
    std::array<T, n1> row(const T (&arr)[n1]) {
        std::array<T, n1> cp;
        std::copy(std::begin(arr), std::end(arr), cp.begin());
        return cp;
    }

    template<typename T, size_t row, size_t column>
    constexpr Mat<T, row, column> make(const std::array<T, column> (&arr)[row]) {
        Mat<T, row, column> result{};
        for (size_t r = 0; r < row; ++r)
            for (size_t c = 0; c < column; ++c)
                result[r][c] = arr[r][c];
        return result;
    }

    template<typename T, size_t row, size_t column, MatrixLayout layout>
    constexpr Mat<T, row, column> make(std::initializer_list<std::initializer_list<T>> init) {
        Mat<T, row, column> result{};
        size_t outer = 0;
        for (const auto& values : init) {
            if (outer == (layout == MatrixLayout::ROW_MAJOR ? row : column))
                break;
            size_t inner = 0;
            for (const auto& value : values) {
                if (inner == (layout == MatrixLayout::ROW_MAJOR ? column : row))
                    break;
                if constexpr (layout == MatrixLayout::ROW_MAJOR)
                    result[outer][inner] = value;
                else
                    result[inner][outer] = value;
                ++inner;
            }
            ++outer;
        }
        return result;
    }

    template<typename R, size_t size>
    Mat<R, size, size> make_identity() {
        Mat<R, size, size> result;
        for (size_t i = 0; i < size; i++)
            for (size_t j = 0; j < size; j++)
                if (i == j)
                    result[i][j] = 1;
                else
                    result[i][j] = 0;
        return result;
    }

    template<matrix::is_matrix R>
    requires(R::Type_().row == R::Type_().column)
    Mat<typename R::ElementType, R::Type_().row, R::Type_().column> make_identity() {
        return make_identity<typename R::ElementType, R::Type_().row>();
    }

    template<typename R, size_t row, size_t column>
    std::expected<Mat<R, row, column>, FailedCause> inverse_gauss_jordan(const Mat<R, row, column>& mat) {
        if constexpr (row != column) {
            return std::unexpected(FailedCause::NOT_SQUARE);
        } else {
            constexpr size_t size = row;
            auto abs_value = [](const R& v) {
                using std::abs;
                return abs(v);
            };
            auto is_near_zero = [&](const R& v) {
                if constexpr (std::is_floating_point_v<R>) {
                    return abs_value(v) <= std::numeric_limits<R>::epsilon() * static_cast<R>(32);
                } else {
                    return v == R{};
                }
            };

            std::array<std::array<R, size * 2>, size> aug{};
            for (size_t i = 0; i < size; ++i) {
                for (size_t j = 0; j < size; ++j) {
                    aug[i][j] = mat[i][j];
                    aug[i][j + size] = (i == j) ? static_cast<R>(1) : static_cast<R>(0);
                }
            }

            std::array<int, size> row_exponents{}, column_exponents{};
            if constexpr (std::is_floating_point_v<R>) {
                auto equilibrate = [&](bool rows, auto& exponents) {
                    bool exact = true;
                    for (size_t i = 0; i < size; ++i) {
                        R largest{};
                        for (size_t j = 0; j < size; ++j)
                            largest = std::max(largest, abs_value(rows ? aug[i][j] : aug[j][i]));
                        if (largest == R{} || !std::isfinite(largest))
                            continue;
                        exponents[i] = std::ilogb(largest);
                        if (exponents[i] == 0)
                            continue;
                        for (size_t j = 0; j < size; ++j) {
                            auto& value = rows ? aug[i][j] : aug[j][i];
                            const R scaled = std::scalbn(value, -exponents[i]);
                            if (std::scalbn(scaled, exponents[i]) != value)
                                exact = false;
                            value = scaled;
                        }
                    }
                    return exact;
                };
                auto restore = [&] {
                    for (size_t i = 0; i < size; ++i)
                        for (size_t j = 0; j < size; ++j)
                            aug[i][j] = mat[i][j];
                    row_exponents.fill(0);
                    column_exponents.fill(0);
                };
                // B = D^-1 A C^-1。2の冪で入力の精度を保ち、単位に依存する小ピボットを避ける。
                // epsilon判定はこの行列に適用するため、ほぼ従属する入力の受理条件も変わる。
                if (!equilibrate(true, row_exponents) || !equilibrate(false, column_exponents)) {
                    restore();
                    if (!equilibrate(false, column_exponents) || !equilibrate(true, row_exponents))
                        restore();
                }
            }

            for (size_t i = 0; i < size; ++i) {
                size_t pivot = i;
                auto max_abs = abs_value(aug[i][i]);
                for (size_t r = i + 1; r < size; ++r) {
                    auto v = abs_value(aug[r][i]);
                    if (v > max_abs) {
                        max_abs = v;
                        pivot = r;
                    }
                }
                if (is_near_zero(max_abs)) {
                    return std::unexpected(FailedCause::SINGULAR);
                }
                if (pivot != i) {
                    std::swap(aug[i], aug[pivot]);
                }

                const R pivot_value = aug[i][i];
                for (size_t j = 0; j < size * 2; ++j)
                    aug[i][j] /= pivot_value;

                for (size_t r = 0; r < size; ++r) {
                    if (r == i)
                        continue;
                    const R factor = aug[r][i];
                    // 微小な非ゼロ係数でも、ピボット行の大きい要素との積は無視できない。
                    if (factor == R{})
                        continue;
                    for (size_t j = 0; j < size * 2; ++j)
                        aug[r][j] -= factor * aug[i][j];
                }
            }

            Mat<R, size, size> inverse{};
            for (size_t i = 0; i < size; ++i)
                for (size_t j = 0; j < size; ++j) {
                    inverse[i][j] = aug[i][j + size];
                    if constexpr (std::is_floating_point_v<R>) {
                        const int exponent = -column_exponents[i] - row_exponents[j];
                        if (exponent != 0)
                            inverse[i][j] = std::scalbn(inverse[i][j], exponent);
                    }
                }

            return inverse;
        }
    }

    template<typename R>
    std::expected<Mat<R, 4, 4>, FailedCause> inverse_affine(const Mat<R, 4, 4>& mat) {
        auto abs_value = [](const R& v) {
            using std::abs;
            return abs(v);
        };
        auto is_near_zero = [&](const R& v) {
            if constexpr (std::is_floating_point_v<R>) {
                return abs_value(v) <= std::numeric_limits<R>::epsilon() * static_cast<R>(32);
            } else {
                return v == R{};
            }
        };
        auto is_near_one = [&](const R& v) {
            return is_near_zero(v - static_cast<R>(1));
        };

        if (!is_near_zero(mat[3][0]) || !is_near_zero(mat[3][1]) || !is_near_zero(mat[3][2]) ||
            !is_near_one(mat[3][3])) {
            return std::unexpected(FailedCause::NON_AFFINE);
        }

        Mat<R, 4, 4> inverse{};
        if constexpr (std::is_floating_point_v<R>) {
            Mat<R, 3, 3> linear{};
            for (size_t i = 0; i < 3; ++i)
                for (size_t j = 0; j < 3; ++j)
                    linear[i][j] = mat[i][j];
            const auto result = inverse_gauss_jordan(linear);
            if (!result)
                return std::unexpected(result.error());
            for (size_t i = 0; i < 3; ++i)
                for (size_t j = 0; j < 3; ++j)
                    inverse[i][j] = (*result)[i][j];
        } else {
            const R a00 = mat[0][0];
            const R a01 = mat[0][1];
            const R a02 = mat[0][2];
            const R a10 = mat[1][0];
            const R a11 = mat[1][1];
            const R a12 = mat[1][2];
            const R a20 = mat[2][0];
            const R a21 = mat[2][1];
            const R a22 = mat[2][2];

            const R det = a00 * (a11 * a22 - a12 * a21) - a01 * (a10 * a22 - a12 * a20) + a02 * (a10 * a21 - a11 * a20);

            if (is_near_zero(det)) {
                return std::unexpected(FailedCause::SINGULAR);
            }

            const R inv_det = static_cast<R>(1) / det;

            inverse[0][0] = (a11 * a22 - a12 * a21) * inv_det;
            inverse[0][1] = (a02 * a21 - a01 * a22) * inv_det;
            inverse[0][2] = (a01 * a12 - a02 * a11) * inv_det;
            inverse[1][0] = (a12 * a20 - a10 * a22) * inv_det;
            inverse[1][1] = (a00 * a22 - a02 * a20) * inv_det;
            inverse[1][2] = (a02 * a10 - a00 * a12) * inv_det;
            inverse[2][0] = (a10 * a21 - a11 * a20) * inv_det;
            inverse[2][1] = (a01 * a20 - a00 * a21) * inv_det;
            inverse[2][2] = (a00 * a11 - a01 * a10) * inv_det;
        }

        const R t0 = mat[0][3];
        const R t1 = mat[1][3];
        const R t2 = mat[2][3];

        inverse[0][3] = -(inverse[0][0] * t0 + inverse[0][1] * t1 + inverse[0][2] * t2);
        inverse[1][3] = -(inverse[1][0] * t0 + inverse[1][1] * t1 + inverse[1][2] * t2);
        inverse[2][3] = -(inverse[2][0] * t0 + inverse[2][1] * t1 + inverse[2][2] * t2);

        inverse[3][0] = static_cast<R>(0);
        inverse[3][1] = static_cast<R>(0);
        inverse[3][2] = static_cast<R>(0);
        inverse[3][3] = static_cast<R>(1);

        return inverse;
    }

} // namespace AHO_NAMESPACE::matrix
