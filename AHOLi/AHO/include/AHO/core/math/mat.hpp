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
        template<typename R, size_t Row, size_t Column>
        struct Mat;

        struct Type {
            size_t row, column;
        };

        enum class FailedCause {
            NotSquare,
            Singular,
            NonAffine
        };

        template<typename T>
        concept to_matrixable = requires(T& t) {
            typename T::element_type;
            { t.type() } -> std::same_as<matrix::Type>;
            { t.to_matrix() } -> std::same_as<Mat<typename T::element_type, T::type().row, T::type().column>>;
        };

        template<typename T>
        concept is_matrix = requires(T& t) {
            { to_matrixable<T> };
            t + std::declval<Mat<typename T::element_type, T::type().row, T::type().column>>();
            t - std::declval<Mat<typename T::element_type, T::type().row, T::type().column>>();
            t * std::declval<Mat<typename T::element_type, T::type().row, T::type().column>>();
            t += std::declval<Mat<typename T::element_type, T::type().row, T::type().column>>();
            t -= std::declval<Mat<typename T::element_type, T::type().row, T::type().column>>();
            t *= std::declval<Mat<typename T::element_type, T::type().row, T::type().column>>();
        };

        namespace detail {
            template<typename L, typename R>
            using product_type = decltype(std::declval<const L&>() * std::declval<const R&>());

            template<typename L, typename R, size_t Rows, size_t Columns>
            concept matrix_product = requires(const L& lhs, const R& rhs) { lhs * rhs; } &&
                                     std::is_object_v<product_type<L, R>> &&
                                     requires(const L& lhs, const R& rhs, product_type<L, R>& total) { total += lhs * rhs; } &&
                                     (Rows == 0 || Columns == 0 || std::default_initializable<product_type<L, R>>);
        } // namespace detail

        template<typename R, size_t Row, size_t Column>
        struct Mat : public std::array<std::array<R, Column>, Row> {
            static constexpr bool __is_mat = true;

            using element_type = R;

            using RowType = std::array<R, Column>;
            using OriginArray = std::array<RowType, Row>;

            static consteval Type type() {
                return {Row, Column};
            }

            constexpr Mat() = default;

            constexpr Mat(const std::array<std::array<R, Column>, Row>& init);

            constexpr Mat(const std::initializer_list<std::initializer_list<R>>& init);

            constexpr AHO_NAMESPACE::matrix::Mat<R, Row, Column> operator+() const;

            constexpr AHO_NAMESPACE::matrix::Mat<R, Row, Column> operator-() const;

            template<typename T>
            requires requires(const R& lhs, const T& rhs) { lhs + rhs; }
            constexpr auto operator+(const AHO_NAMESPACE::matrix::Mat<T, Row, Column>& p) const;

            template<typename T>
            requires requires(const R& lhs, const T& rhs) { lhs - rhs; }
            constexpr auto operator-(const AHO_NAMESPACE::matrix::Mat<T, Row, Column>& p) const;

            template<typename T, size_t TargetColumn>
            requires detail::matrix_product<R, T, Row, TargetColumn>
            constexpr auto operator*(const AHO_NAMESPACE::matrix::Mat<T, Column, TargetColumn>& p) const;

            template<typename T>
            requires requires(R& lhs, const T& rhs) { lhs += rhs; }
            constexpr Mat<R, Row, Column>& operator+=(const AHO_NAMESPACE::matrix::Mat<T, Row, Column>& p);

            template<typename T>
            requires requires(R& lhs, const T& rhs) { lhs -= rhs; }
            constexpr Mat<R, Row, Column>& operator-=(const AHO_NAMESPACE::matrix::Mat<T, Row, Column>& p);

            template<typename T>
            requires(Row == Column) && detail::matrix_product<R, T, Row, Column> &&
                    requires(R& target, const detail::product_type<R, T>& value) { target = value; }
            constexpr Mat<R, Row, Column>& operator*=(const AHO_NAMESPACE::matrix::Mat<T, Row, Column>& p);

            template<typename T>
            requires requires(R& lhs, const T& rhs) { lhs *= rhs; }
            constexpr Mat<R, Row, Column>& operator*=(const T& m);

            template<typename T>
            requires requires(R& lhs, const T& rhs) { lhs /= rhs; }
            constexpr Mat<R, Row, Column>& operator/=(const T& m);

            template<typename T>
            requires AHO_NAMESPACE::concepts::cast_as<R, T>
            Mat<T, Row, Column> cast() const;
        };

        template<typename T, size_t N1>
        std::array<T, N1> row(const T (&arr)[N1]);

        template<typename T, size_t Row, size_t Column>
        constexpr Mat<T, Row, Column> make(const std::array<T, Column> (&arr)[Row]);

        template<typename T, size_t Row, size_t Column>
        constexpr Mat<T, Row, Column> make(std::initializer_list<std::initializer_list<T>>);

        template<typename L, size_t Row, size_t Column, typename R>
        requires(!requires {{ R::__is_mat } -> std::convertible_to<bool>; }) && requires(const L& lhs, const R& rhs) { lhs * rhs; }
        constexpr auto operator*(const matrix::Mat<L, Row, Column>& mat, const R& m);

        template<typename L, size_t Row, size_t Column, typename R>
        requires(!requires {{ R::__is_mat } -> std::convertible_to<bool>; }) && requires(const L& lhs, const R& rhs) { lhs / rhs; }
        constexpr auto operator/(const matrix::Mat<L, Row, Column>& mat, const R& m);

        template<typename L, size_t Row, size_t Column, typename R>
        requires(!requires {{ R::__is_mat } -> std::convertible_to<bool>; }) && requires(const R& lhs, const L& rhs) { lhs * rhs; }
        constexpr auto operator*(const R& m, const matrix::Mat<L, Row, Column>& mat);

        template<typename L, size_t Row, size_t Column, typename R>
        requires(!requires {{ R::__is_mat } -> std::convertible_to<bool>; }) && requires(const R& lhs, const L& rhs) { lhs / rhs; }
        constexpr auto operator/(const R& m, const matrix::Mat<L, Row, Column>& mat);
    } // namespace matrix

    template<typename R, size_t Row, size_t Column>
    using Mat = matrix::Mat<R, Row, Column>;

    template<typename R>
    using Mat2x2 = matrix::Mat<R, 2, 2>;

    template<typename R>
    using Mat4x4 = matrix::Mat<R, 4, 4>;

    using Mat2x2F = matrix::Mat<float, 2, 2>;

    using Mat4x4F = matrix::Mat<float, 4, 4>;

    namespace matrix {
        template<typename R, size_t Size>
        Mat<R, Size, Size> make_identity();

        template<typename R = float, typename Dim = VSL_NAMESPACE::D3>
        requires std::same_as<Dim, VSL_NAMESPACE::D3>
        Mat4x4<R> make_move(_Vector<R, Dim> vec);

        template<matrix::is_matrix R>
        requires(R::type().row == R::type().column)
        Mat<typename R::element_type, R::type().row, R::type().column> make_identity();

        template<typename R = float>
        Mat2x2<R> make_rotation(angle::Radian rad);

        template<typename R = float>
        Mat2x2<R> make_rotation(angle::Degree deg);

        template<typename R = float, typename CI = AHO_NAMESPACE::coordinate::_DefaultCoordinateInfo<VSL_NAMESPACE::D3>::value>
        requires std::same_as<typename CI::dimension, VSL_NAMESPACE::D3>
        Mat4x4<R> make_rotation(angle::Radian rad, _Vector<R, VSL_NAMESPACE::D3, CI> axis);

        template<typename R = float, typename CI = AHO_NAMESPACE::coordinate::_DefaultCoordinateInfo<VSL_NAMESPACE::D3>::value>
        requires std::same_as<typename CI::dimension, VSL_NAMESPACE::D3>
        Mat4x4<R> make_rotation(angle::Degree deg, _Vector<R, VSL_NAMESPACE::D3, CI> axis);

        template<typename R = float, typename CI = AHO_NAMESPACE::coordinate::_DefaultCoordinateInfo<VSL_NAMESPACE::D3>::value>
        requires std::same_as<typename CI::dimension, VSL_NAMESPACE::D3>
        Mat4x4<R> make_view(_Point<R, typename CI::dimension, CI> eye, _Point<R, typename CI::dimension, CI> target, _Vector<R, typename CI::dimension, CI> up);

        template<typename T>
        Mat4x4<T> make_perspective(Radian fov, T aspect, T near, T far);

        template<typename T>
        Mat4x4<T> make_perspective(Degree fov, T aspect, T near, T far);

        template<typename R, size_t Row, size_t Column>
        std::expected<Mat<R, Row, Column>, FailedCause> inverse_gauss_jordan(const Mat<R, Row, Column>& mat);

        template<typename R>
        std::expected<Mat<R, 4, 4>, FailedCause> inverse_affine(const Mat<R, 4, 4>& mat);
    } // namespace matrix
    //------------------------------------------------------------------------------------------

    namespace matrix {
        template<typename R, size_t Row, size_t Column>
        constexpr Mat<R, Row, Column>::Mat(const std::array<std::array<R, Column>, Row>& init)
            : std::array<std::array<R, Column>, Row>{init} {}

        template<typename R, size_t Row, size_t Column>
        constexpr Mat<R, Row, Column>::Mat(const std::initializer_list<std::initializer_list<R>>& init)
            : OriginArray{} {
            size_t row = 0;
            for (const auto& source_row : init) {
                if (row == Row)
                    break;
                size_t column = 0;
                for (const auto& value : source_row) {
                    if (column == Column)
                        break;
                    (*this)[row][column++] = value;
                }
                ++row;
            }
        }

        template<typename R, size_t Row, size_t Column>
        constexpr AHO_NAMESPACE::matrix::Mat<R, Row, Column> Mat<R, Row, Column>::operator+() const {
            return *this;
        }

        template<typename R, size_t Row, size_t Column>
        constexpr AHO_NAMESPACE::matrix::Mat<R, Row, Column> Mat<R, Row, Column>::operator-() const {
            Mat<R, Row, Column> buf;
            for (size_t i = 0; i < Row; i++)
                for (size_t j = 0; j < Column; j++)
                    buf[i][j] = -(*this)[i][j];
            return std::move(buf);
        }

        template<typename R, size_t Row, size_t Column>
        template<typename T>
        requires requires(const R& lhs, const T& rhs) { lhs + rhs; }
        constexpr auto Mat<R, Row, Column>::operator+(const Mat<T, Row, Column>& p) const {
            Mat<decltype((*this)[0][0] + p[0][0]), Row, Column> buf;
            for (size_t i = 0; i < Row; i++)
                for (size_t j = 0; j < Column; j++)
                    buf[i][j] = (*this)[i][j] + p[i][j];
            return std::move(buf);
        }

        template<typename R, size_t Row, size_t Column>
        template<typename T>
        requires requires(const R& lhs, const T& rhs) { lhs - rhs; }
        constexpr auto Mat<R, Row, Column>::operator-(const Mat<T, Row, Column>& p) const {
            Mat<decltype((*this)[0][0] - p[0][0]), Row, Column> buf;
            for (size_t i = 0; i < Row; i++)
                for (size_t j = 0; j < Column; j++)
                    buf[i][j] = (*this)[i][j] - p[i][j];
            return std::move(buf);
        }

        template<typename R, size_t Row, size_t Column>
        template<typename T, size_t TargetColumn>
        requires detail::matrix_product<R, T, Row, TargetColumn>
        constexpr auto
        Mat<R, Row, Column>::operator*(const AHO_NAMESPACE::matrix::Mat<T, Column, TargetColumn>& p) const {
            AHO_NAMESPACE::matrix::Mat<decltype((*this)[0][0] * p[0][0]), Row, TargetColumn> buf{};

            for (size_t i = 0; i < Row; i++)
                for (size_t j = 0; j < TargetColumn; j++)
                    for (size_t k = 0; k < Column; k++)
                        buf[i][j] += (*this)[i][k] * p[k][j];
            return std::move(buf);
        }

        template<typename R, size_t Row, size_t Column>
        template<typename T>
        requires requires(R& lhs, const T& rhs) { lhs += rhs; }
        constexpr Mat<R, Row, Column>&
        Mat<R, Row, Column>::operator+=(const AHO_NAMESPACE::matrix::Mat<T, Row, Column>& p) {
            for (size_t i = 0; i < Row; i++)
                for (size_t j = 0; j < Column; j++)
                    (*this)[i][j] += p[i][j];
            return *this;
        }

        template<typename R, size_t Row, size_t Column>
        template<typename T>
        requires requires(R& lhs, const T& rhs) { lhs -= rhs; }
        constexpr Mat<R, Row, Column>&
        Mat<R, Row, Column>::operator-=(const AHO_NAMESPACE::matrix::Mat<T, Row, Column>& p) {
            for (size_t i = 0; i < Row; i++)
                for (size_t j = 0; j < Column; j++)
                    (*this)[i][j] -= p[i][j];
            return *this;
        }

        template<typename R, size_t Row, size_t Column>
        template<typename T>
        requires(Row == Column) && detail::matrix_product<R, T, Row, Column> &&
                requires(R& target, const detail::product_type<R, T>& value) { target = value; }
        constexpr Mat<R, Row, Column>&
        Mat<R, Row, Column>::operator*=(const AHO_NAMESPACE::matrix::Mat<T, Row, Column>& p) {
            const auto product = *this * p;
            for (size_t i = 0; i < Row; i++)
                for (size_t j = 0; j < Column; j++)
                    (*this)[i][j] = product[i][j];
            return *this;
        }

        template<typename R, size_t Row, size_t Column>
        template<typename T>
        requires requires(R& lhs, const T& rhs) { lhs *= rhs; }
        constexpr Mat<R, Row, Column>& Mat<R, Row, Column>::operator*=(const T& m) {
            for (size_t i = 0; i < Row; i++)
                for (size_t j = 0; j < Column; j++)
                    (*this)[i][j] *= m;
            return *this;
        }

        template<typename R, size_t Row, size_t Column>
        template<typename T>
        requires requires(R& lhs, const T& rhs) { lhs /= rhs; }
        constexpr Mat<R, Row, Column>& Mat<R, Row, Column>::operator/=(const T& m) {
            for (size_t i = 0; i < Row; i++)
                for (size_t j = 0; j < Column; j++)
                    (*this)[i][j] /= m;
            return *this;
        }

        template<typename R, size_t Row, size_t Column>
        template<typename T>
        requires AHO_NAMESPACE::concepts::cast_as<R, T>
        Mat<T, Row, Column> Mat<R, Row, Column>::cast() const {
            AHO_NAMESPACE::matrix::Mat<T, Row, Column> buf;
            for (size_t i = 0; i < Row; i++)
                for (size_t j = 0; j < Column; j++)
                    buf[i][j] = (T)(*this)[i][j];
            return std::move(buf);
        }

        template<typename R, size_t Row, size_t Column, typename T>
        requires(!requires {{ T::__is_mat } -> std::convertible_to<bool>; }) && requires(const R& lhs, const T& rhs) { lhs * rhs; }
        constexpr auto operator*(const Mat<R, Row, Column>& mat, const T& m) {
            AHO_NAMESPACE::matrix::Mat<decltype(mat[0][0] * m), Row, Column> buf;
            for (size_t i = 0; i < Row; i++)
                for (size_t j = 0; j < Column; j++)
                    buf[i][j] = mat[i][j] * m;
            return std::move(buf);
        }

        template<typename R, size_t Row, size_t Column, typename T>
        requires(!requires {{ T::__is_mat } -> std::convertible_to<bool>; }) && requires(const R& lhs, const T& rhs) { lhs / rhs; }
        constexpr auto operator/(const Mat<R, Row, Column>& mat, const T& m) {
            AHO_NAMESPACE::matrix::Mat<decltype(mat[0][0] / m), Row, Column> buf;

            for (size_t i = 0; i < Row; i++)
                for (size_t j = 0; j < Column; j++)
                    buf[i][j] = mat[i][j] / m;
            return std::move(buf);
        }

        template<typename R, size_t Row, size_t Column, typename T>
        requires(!requires {{ T::__is_mat } -> std::convertible_to<bool>; }) && requires(const T& lhs, const R& rhs) { lhs * rhs; }
        constexpr auto operator*(const T& m, const Mat<R, Row, Column>& mat) {
            AHO_NAMESPACE::matrix::Mat<decltype(m * mat[0][0]), Row, Column> buf;

            for (size_t i = 0; i < Row; i++)
                for (size_t j = 0; j < Column; j++)
                    buf[i][j] = m * mat[i][j];
            return std::move(buf);
        }

        template<typename R, size_t Row, size_t Column, typename T>
        requires(!requires {{ T::__is_mat } -> std::convertible_to<bool>; }) && requires(const T& lhs, const R& rhs) { lhs / rhs; }
        constexpr auto operator/(const T& m, const Mat<R, Row, Column>& mat) {
            AHO_NAMESPACE::matrix::Mat<decltype(m / mat[0][0]), Row, Column> buf;

            for (size_t i = 0; i < Row; i++)
                for (size_t j = 0; j < Column; j++)
                    buf[i][j] = m / mat[i][j];
            return std::move(buf);
        }

        template<typename T, size_t N1>
        std::array<T, N1> row(const T (&arr)[N1]) {
            std::array<T, N1> cp;
            std::copy(std::begin(arr), std::end(arr), cp.begin());
            return cp;
        }

        template<typename T, size_t Row, size_t Column>
        constexpr Mat<T, Row, Column> make(const std::array<T, Column> (&arr)[Row]) {
            std::array<std::array<T, Column>, Row> cp;
            std::copy(std::begin(arr), std::end(arr), cp.begin());
            return {cp};
        }

        template<typename T, size_t Row, size_t Column>
        constexpr Mat<T, Row, Column> make(std::initializer_list<std::initializer_list<T>> init) {
            std::array<std::array<T, Column>, Row> cp{};
            auto itr1 = init.begin();
            // Each inner initializer list supplies a column, as in the existing square factory.
            for (size_t i = 0; init.end() != itr1 && i < Column; i++) {
                auto itr2 = itr1->begin();
                for (size_t j = 0; itr2 != itr1->end() && j < Row; j++) {
                    cp[j][i] = *itr2;
                    ++itr2;
                }
                ++itr1;
            }
            return cp;
        }

        template<typename R, size_t Size>
        Mat<R, Size, Size> make_identity() {
            Mat<R, Size, Size> result;
            for (size_t i = 0; i < Size; i++)
                for (size_t j = 0; j < Size; j++)
                    if (i == j)
                        result[i][j] = 1;
                    else
                        result[i][j] = 0;
            return result;
        }

        template<matrix::is_matrix R>
        requires(R::type().row == R::type().column)
        Mat<typename R::element_type, R::type().row, R::type().column> make_identity() {
            return make_identity<typename R::element_type, R::type().row>();
        }

        template<typename R, typename Dim>
        requires std::same_as<Dim, VSL_NAMESPACE::D3>
        Mat4x4<R> make_move(_Vector<R, Dim> vec) {
            return make<R, 4, 4>({{1, 0, 0, vec.value.x.value},
                                  {0, 1, 0, vec.value.y.value},
                                  {0, 0, 1, vec.value.z.value},
                                  {0, 0, 0, 1}});
        }

        template<typename R>
        Mat2x2<R> make_rotation(angle::Radian rad) {
            R c = std::cos(rad.value);
            R s = std::sin(rad.value);
            return Mat2x2<R>({{c, -s},
                              {s, c}});
        }

        template<typename R>
        Mat2x2<R> make_rotation(angle::Degree deg) {
            return make_rotation<R>(Radian(deg));
        }

        template<typename R, typename CI>
        requires std::same_as<typename CI::dimension, VSL_NAMESPACE::D3>
        Mat4x4<R> make_rotation(angle::Radian rad, _Vector<R, VSL_NAMESPACE::D3, CI> axis) {
            axis.normalize(); // 安全のため正規化
            R c = cos(rad.value);
            R s = sin(rad.value);
            R one_c = 1.0f - c;

            R x = axis.value.x.value;
            R y = axis.value.y.value;
            R z = axis.value.z.value;

            return make({std::array<R, 4>{c + x * x * one_c, x * y * one_c - z * s, x * z * one_c + y * s, 0.0f},
                         std::array<R, 4>{y * x * one_c + z * s, c + y * y * one_c, y * z * one_c - x * s, 0.0f},
                         std::array<R, 4>{z * x * one_c - y * s, z * y * one_c + x * s, c + z * z * one_c, 0.0f},
                         std::array<R, 4>{0.0f, 0.0f, 0.0f, 1.0f}});
        }

        template<typename R, typename CI>
        requires std::same_as<typename CI::dimension, VSL_NAMESPACE::D3>
        Mat4x4<R> make_rotation(angle::Degree deg, _Vector<R, VSL_NAMESPACE::D3, CI> axis) {
            return make_rotation(Radian(deg), axis);
        }

        template<typename R, typename CI>
        requires std::same_as<typename CI::dimension, VSL_NAMESPACE::D3>
        Mat4x4<R> make_view(_Point<R, typename CI::dimension, CI> _eye, _Point<R, typename CI::dimension, CI> target, _Vector<R, typename CI::dimension, CI> up) {
            using D = typename CI::dimension;
            _Vector<R, D, CI> eye(_eye.value);
            auto f = _Vector<R, D, CI>(target.value - eye.value).normalize().value; // forward
            // Preserve AHO's existing camera roll; changing to cross(up) reverses both screen axes.
            auto s = _Vector<R, D, CI>(f).cross(-up).normalize().value;                  // right
            auto u = _Vector<R, D, CI>(s).cross(_Vector<R, D, CI>(f)).normalize().value; // up (recomputed)

            Mat4x4<R> result = make_identity<R, 4>();
            result[0][0] = s.x.value;
            result[1][0] = s.y.value;
            result[2][0] = s.z.value;
            result[0][1] = u.x.value;
            result[1][1] = u.y.value;
            result[2][1] = u.z.value;
            result[0][2] = -f.x.value;
            result[1][2] = -f.y.value;
            result[2][2] = -f.z.value;
            result[3][0] = -_Vector<R, D, CI>(s).dot(eye);
            result[3][1] = -_Vector<R, D, CI>(u).dot(eye);
            result[3][2] = _Vector<R, D, CI>(f).dot(eye);
            return result;
        }

        template<typename R>
        Mat4x4<R> make_perspective(Radian fov, R aspect, R near, R far) {
            R f = static_cast<R>(1.0) / std::tan((fov * static_cast<R>(0.5)).value);
            R nf = static_cast<R>(1.0) / (near - far);

            return make({std::array<R, 4>{f / aspect, 0.0f, 0.0f, 0.0f},
                         std::array<R, 4>{0.0f, f, 0.0f, 0.0f},
                         std::array<R, 4>{0.0f, 0.0f, (far + near) * nf, -1.0f},
                         std::array<R, 4>{0.0f, 0.0f, (2 * far * near) * nf, 0.0f}});
        }

        template<typename T>
        Mat4x4<T> make_perspective(Degree fov, T aspect, T near, T far) {
            return make_perspective((Radian)fov, aspect, near, far);
        }

        template<typename R, size_t Row, size_t Column>
        std::expected<Mat<R, Row, Column>, FailedCause> inverse_gauss_jordan(const Mat<R, Row, Column>& mat) {
            if constexpr (Row != Column) {
                return std::unexpected(FailedCause::NotSquare);
            } else {
                constexpr size_t Size = Row;
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

                std::array<std::array<R, Size * 2>, Size> aug{};
                for (size_t i = 0; i < Size; ++i) {
                    for (size_t j = 0; j < Size; ++j) {
                        aug[i][j] = mat[i][j];
                        aug[i][j + Size] = (i == j) ? static_cast<R>(1) : static_cast<R>(0);
                    }
                }

                std::array<int, Size> row_exponents{}, column_exponents{};
                if constexpr (std::is_floating_point_v<R>) {
                    auto equilibrate = [&](bool rows, auto& exponents) {
                        bool exact = true;
                        for (size_t i = 0; i < Size; ++i) {
                            R largest{};
                            for (size_t j = 0; j < Size; ++j)
                                largest = std::max(largest, abs_value(rows ? aug[i][j] : aug[j][i]));
                            if (largest == R{} || !std::isfinite(largest))
                                continue;
                            exponents[i] = std::ilogb(largest);
                            if (exponents[i] == 0)
                                continue;
                            for (size_t j = 0; j < Size; ++j) {
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
                        for (size_t i = 0; i < Size; ++i)
                            for (size_t j = 0; j < Size; ++j)
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

                for (size_t i = 0; i < Size; ++i) {
                    size_t pivot = i;
                    auto max_abs = abs_value(aug[i][i]);
                    for (size_t r = i + 1; r < Size; ++r) {
                        auto v = abs_value(aug[r][i]);
                        if (v > max_abs) {
                            max_abs = v;
                            pivot = r;
                        }
                    }
                    if (is_near_zero(max_abs)) {
                        return std::unexpected(FailedCause::Singular);
                    }
                    if (pivot != i) {
                        std::swap(aug[i], aug[pivot]);
                    }

                    const R pivot_value = aug[i][i];
                    for (size_t j = 0; j < Size * 2; ++j)
                        aug[i][j] /= pivot_value;

                    for (size_t r = 0; r < Size; ++r) {
                        if (r == i)
                            continue;
                        const R factor = aug[r][i];
                        // A small nonzero factor can still multiply a large pivot-row element.
                        if (factor == R{})
                            continue;
                        for (size_t j = 0; j < Size * 2; ++j)
                            aug[r][j] -= factor * aug[i][j];
                    }
                }

                Mat<R, Size, Size> inverse{};
                for (size_t i = 0; i < Size; ++i)
                    for (size_t j = 0; j < Size; ++j) {
                        inverse[i][j] = aug[i][j + Size];
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
                return std::unexpected(FailedCause::NonAffine);
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
                    return std::unexpected(FailedCause::Singular);
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

        template<typename R, size_t Row, size_t Column>
        std::ostream& operator<<(std::ostream& out, const Mat<R, Row, Column>& mat) {
            out << "{";
            for (size_t i = 0; i < Row; i++) {
                out << "{";
                for (size_t j = 0; j < Column; j++) {
                    if (j != 0)
                        out << ", ";
                    out << mat[i][j];
                }
                out << (i + 1 == Row ? "}}" : "}") << std::endl;
            }
            if constexpr (Row == 0)
                out << "}";
            return out;
        };
    } // namespace matrix
} // namespace AHO_NAMESPACE
