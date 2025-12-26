#pragma once

#include "../../define.hpp"

#include <array>
#include <algorithm>
#include <optional>
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

        template<typename T>
        concept to_matrixable = requires(T &t) {
            typename T::element_type;
            { t.type() } -> std::same_as<matrix::Type>;
            { t.to_matrix() } -> std::same_as<Mat<typename T::element_type, T::type().row, T::type().column>>;
        };

        template<typename T>
        concept is_matrix = requires(T &t) {
            { to_matrixable<T> };
            t + std::declval<Mat<typename T::element_type, T::type().row, T::type().column>>();
            t - std::declval<Mat<typename T::element_type, T::type().row, T::type().column>>();
            t * std::declval<Mat<typename T::element_type, T::type().row, T::type().column>>();
            t += std::declval<Mat<typename T::element_type, T::type().row, T::type().column>>();
            t -= std::declval<Mat<typename T::element_type, T::type().row, T::type().column>>();
            t *= std::declval<Mat<typename T::element_type, T::type().row, T::type().column>>();
        };

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

            constexpr Mat(const std::array<std::array<R, Column>, Row> &init);

            constexpr Mat(const std::initializer_list<std::initializer_list<R>> &init);

            constexpr AHO_NAMESPACE::matrix::Mat<R, Row, Column> operator+() const;

            constexpr AHO_NAMESPACE::matrix::Mat<R, Row, Column> operator-() const;

            template<AHO_NAMESPACE::concepts::add_as<R> T>
            constexpr auto operator+(const AHO_NAMESPACE::matrix::Mat<T, Row, Column> &p) const;

            template<AHO_NAMESPACE::concepts::subtract_as<R> T>
            constexpr auto operator-(const AHO_NAMESPACE::matrix::Mat<T, Row, Column> &p) const;

            template<AHO_NAMESPACE::concepts::multiply_as<R> T, size_t TargetColumn>
            constexpr auto operator*(const AHO_NAMESPACE::matrix::Mat<T, Column, TargetColumn> &p) const;

            template<AHO_NAMESPACE::concepts::add_as<R> T>
            constexpr Mat<R, Row, Column> &operator+=(const AHO_NAMESPACE::matrix::Mat<T, Row, Column> &p);

            template<AHO_NAMESPACE::concepts::subtract_as<R> T>
            constexpr Mat<R, Row, Column> &operator-=(const AHO_NAMESPACE::matrix::Mat<T, Row, Column> &p);

            template<AHO_NAMESPACE::concepts::multiply_as<R> T>
            constexpr Mat<R, Row, Column> &operator*=(const AHO_NAMESPACE::matrix::Mat<T, Row, Column> &p);

            template<AHO_NAMESPACE::concepts::multiply_as<R> T>
            constexpr Mat<R, Row, Column> operator*=(const T &m);

            template<AHO_NAMESPACE::concepts::division_as<R> T>
            constexpr Mat<R, Row, Column> operator/=(const T &m);

            template<AHO_NAMESPACE::concepts::cast_as<R> T>
            Mat<T, Row, Column> cast();
        };

        template<typename T, size_t N1>
        std::array<T, N1> row(const T(&arr)[N1]);

        template<typename T, size_t Row, size_t Column>
        constexpr Mat<T, Row, Column> make(const std::array<T, Column> (&arr)[Row]);

        template<typename T, size_t Row, size_t Column>
        constexpr Mat<T, Row, Column> make(std::initializer_list<std::initializer_list<T>>);

        template<typename L, size_t Row, size_t Column, AHO_NAMESPACE::concepts::multiply_as<L> R>
        requires (!requires {{ R::__is_mat } -> std::convertible_to<bool>; })
        constexpr matrix::Mat<L, Row, Column> operator*(const matrix::Mat<L, Row, Column> &mat, const R &m);

        template<typename L, size_t Row, size_t Column, AHO_NAMESPACE::concepts::division_as<L> R>
        requires (!requires {{ R::__is_mat } -> std::convertible_to<bool>; })
        constexpr matrix::Mat<L, Row, Column> operator/(const matrix::Mat<L, Row, Column> &mat, const R &m);

        template<typename L, size_t Row, size_t Column, AHO_NAMESPACE::concepts::multiply_as<L> R>
        requires (!requires {{ R::__is_mat } -> std::convertible_to<bool>; })
        constexpr matrix::Mat<L, Row, Column> operator*(const R &m, const matrix::Mat<L, Row, Column> &mat);

        template<typename L, size_t Row, size_t Column, AHO_NAMESPACE::concepts::division_as<L> R>
        requires (!requires {{ R::__is_mat } -> std::convertible_to<bool>; })
        constexpr matrix::Mat<L, Row, Column> operator/(const R &m, const matrix::Mat<L, Row, Column> &mat);
    }

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
        requires (R::type().row == R::type().column)
        Mat<typename R::element_type, R::type().row, R::type().column> make_identity();

        template<typename R = float>
        Mat2x2<R> make_rotation(angle::Radian rad);

        template<typename R = float>
        Mat2x2<R> make_rotation(angle::Degree deg);

        template<typename R = float, typename CI = AHO_NAMESPACE::coordinate::_DefaultCoordinateInfo<VSL_NAMESPACE::D3>::value>
        Mat4x4<R> make_rotation(angle::Radian rad, _Vector<R, CI> axis);

        template<typename R = float, typename CI = AHO_NAMESPACE::coordinate::_DefaultCoordinateInfo<VSL_NAMESPACE::D3>::value>
        requires std::same_as<typename CI::dimension, VSL_NAMESPACE::D3>
        Mat4x4<R> make_rotation(angle::Degree deg, _Vector<R, CI> axis);

        template<typename R = float, typename CI = AHO_NAMESPACE::coordinate::_DefaultCoordinateInfo<VSL_NAMESPACE::D3>::value>
        requires std::same_as<typename CI::dimension, VSL_NAMESPACE::D3>
        Mat4x4<R> make_view(_Point<R, typename CI::dimention, CI> eye, _Point<R, typename CI::dimention, CI> target, _Vector<R, typename CI::dimention, CI> up);

        template<typename T>
        Mat4x4<T> make_perspective(Radian fov, T aspect, T near, T far);

        template<typename T>
        Mat4x4<T> make_perspective(Degree fov, T aspect, T near, T far);
    }
    //------------------------------------------------------------------------------------------

    namespace matrix {
        template<typename R, size_t Row, size_t Column>
        constexpr Mat<R, Row, Column>::Mat(const std::array<std::array<R, Column>, Row> &init)
                : std::array<std::array<R, Column>, Row>{init} {}

        template<typename R, size_t Row, size_t Column>
        constexpr Mat<R, Row, Column>::Mat(const std::initializer_list<std::initializer_list<R>> &init) {
            int i = 0, j = 0;
            for (auto itr1: init) {
                for (auto itr2: itr1) {
                    (*this)[i][j] = itr2;
                    j++;
                    if (Column <= j)
                        break;
                }
                i++;
                if (Row <= i)
                    break;
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
        template<AHO_NAMESPACE::concepts::add_as<R> T>
        constexpr auto Mat<R, Row, Column>::operator+(const Mat<T, Row, Column> &p) const {
            Mat<decltype(std::declval<R>() + std::declval<T>()), Row, Column> buf;
            for (size_t i = 0; i < Row; i++)
                for (size_t j = 0; j < Column; j++)
                    buf[i][j] = (*this)[i][j] + p[i][j];
            return std::move(buf);
        }

        template<typename R, size_t Row, size_t Column>
        template<AHO_NAMESPACE::concepts::subtract_as<R> T>
        constexpr auto Mat<R, Row, Column>::operator-(const Mat<T, Row, Column> &p) const {
            Mat<decltype(std::declval<R>() - std::declval<T>()), Row, Column> buf;
            for (size_t i = 0; i < Row; i++)
                for (size_t j = 0; j < Column; j++)
                    buf[i][j] = (*this)[i][j] - p[i][j];
            return std::move(buf);
        }

        template<typename R, size_t Row, size_t Column>
        template<AHO_NAMESPACE::concepts::multiply_as<R> T, size_t TargetColumn>
        constexpr auto
        Mat<R, Row, Column>::operator*(const AHO_NAMESPACE::matrix::Mat<T, Column, TargetColumn> &p) const {
            AHO_NAMESPACE::matrix::Mat<decltype(std::declval<T>() * std::declval<R>()), Row, TargetColumn> buf {};

            for (size_t i = 0; i < Row; i++)
                for (size_t j = 0; j < TargetColumn; j++)
                    for (size_t k = 0; k < Column; k++)
                        buf[i][j] += (*this)[i][k] * p[k][j];
            return std::move(buf);
        }

        template<typename R, size_t Row, size_t Column>
        template<AHO_NAMESPACE::concepts::add_as<R> T>
        constexpr Mat<R, Row, Column> &
        Mat<R, Row, Column>::operator+=(const AHO_NAMESPACE::matrix::Mat<T, Row, Column> &p) {
            for (size_t i = 0; i < Row; i++)
                for (size_t j = 0; j < Column; j++)
                    (*this)[i][j] += p[i][j];
            return *this;
        }

        template<typename R, size_t Row, size_t Column>
        template<AHO_NAMESPACE::concepts::subtract_as<R> T>
        constexpr Mat<R, Row, Column> &
        Mat<R, Row, Column>::operator-=(const AHO_NAMESPACE::matrix::Mat<T, Row, Column> &p) {
            for (size_t i = 0; i < Row; i++)
                for (size_t j = 0; j < Column; j++)
                    (*this)[i][j] + -p[i][j];
            return *this;
        }

        template<typename R, size_t Row, size_t Column>
        template<AHO_NAMESPACE::concepts::multiply_as<R> T>
        constexpr Mat<R, Row, Column> &
        Mat<R, Row, Column>::operator*=(const AHO_NAMESPACE::matrix::Mat<T, Row, Column> &p) {
            // static_assert(Row == Column, "different type  (*= -> *)");

            for (size_t i = 0; i < Row; i++)
                for (size_t j = 0; j < Column; j++)
                    for (size_t k = 0; k < Column; k++)
                        (*this)[i][k] += p[k][j];
            return *this;
        }

        template<typename R, size_t Row, size_t Column>
        template<AHO_NAMESPACE::concepts::multiply_as<R> T>
        constexpr Mat<R, Row, Column> Mat<R, Row, Column>::operator*=(const T &m) {
            for (size_t i = 0; i < Row; i++)
                for (size_t j = 0; j < Column; j++)
                    (*this)[i][j] *= m;
            return *this;
        }

        template<typename R, size_t Row, size_t Column>
        template<AHO_NAMESPACE::concepts::division_as<R> T>
        constexpr Mat<R, Row, Column> Mat<R, Row, Column>::operator/=(const T &m) {
            for (size_t i = 0; i < Row; i++)
                for (size_t j = 0; j < Column; j++)
                    (*this)[i][j] /= m;
            return *this;
        }

        template<typename R, size_t Row, size_t Column>
        template<AHO_NAMESPACE::concepts::cast_as<R> T>
        Mat<T, Row, Column> Mat<R, Row, Column>::cast() {
            AHO_NAMESPACE::matrix::Mat<T, Row, Column> buf;
            for (size_t i = 0; i < Row; i++)
                for (size_t j = 0; j < Column; j++)
                    buf[i][j] = (T)(*this)[i][j];
            return std::move(buf);
        }

        template<typename R, size_t Row, size_t Column, AHO_NAMESPACE::concepts::multiply_as<R> T>
        requires (!requires {{ R::__is_mat } -> std::convertible_to<bool>; })
        constexpr Mat<R, Row, Column> operator*(const Mat<R, Row, Column> &mat, const T &m) {
            AHO_NAMESPACE::matrix::Mat<decltype(std::declval<T>() * std::declval<R>()), Row, Column> buf;
            for (size_t i = 0; i < Row; i++)
                for (size_t j = 0; j < Column; j++)
                    buf[i][j] = mat[i][j] * m;
            return std::move(buf);
        }

        template<typename R, size_t Row, size_t Column, AHO_NAMESPACE::concepts::division_as<R> T>
        requires (!requires {{ R::__is_mat } -> std::convertible_to<bool>; })
        constexpr Mat<R, Row, Column> operator/(const Mat<R, Row, Column> &mat, const T &m) {
            AHO_NAMESPACE::matrix::Mat<decltype(std::declval<T>() / std::declval<R>()), Row, Column> buf;

            for (size_t i = 0; i < Row; i++)
                for (size_t j = 0; j < Column; j++)
                    buf[i][j] = mat[i][j] / m;
            return std::move(buf);
        }

        template<typename R, size_t Row, size_t Column, AHO_NAMESPACE::concepts::multiply_as<R> T>
        requires (!requires {{ R::__is_mat } -> std::convertible_to<bool>; })
        constexpr Mat<R, Row, Column> operator*(const T &m, const Mat<R, Row, Column> &mat) {
            AHO_NAMESPACE::matrix::Mat<decltype(std::declval<T>() * std::declval<R>()), Row, Column> buf;

            for (size_t i = 0; i < Row; i++)
                for (size_t j = 0; j < Column; j++)
                    buf[i][j] = mat[i][j] * m;
            return std::move(buf);
        }

        template<typename R, size_t Row, size_t Column, AHO_NAMESPACE::concepts::division_as<R> T>
        requires (!requires {{ R::__is_mat } -> std::convertible_to<bool>; })
        constexpr Mat<R, Row, Column> operator/(const T &m, const Mat<R, Row, Column> &mat) {
            AHO_NAMESPACE::matrix::Mat<decltype(std::declval<T>() / std::declval<R>()), Row, Column> buf;

            for (size_t i = 0; i < Row; i++)
                for (size_t j = 0; j < Column; j++)
                    buf[i][j] = mat[i][j] / m;
            return std::move(buf);
        }

        template<typename T, size_t N1>
        std::array<T, N1> row(const T(&arr)[N1]) {
            std::array<T, N1> cp;
            std::copy(std::begin(arr), std::end(arr), cp.begin());
            return cp;
        }

        template<typename T, size_t Row, size_t Column>
        constexpr Mat<T, Row, Column> make(const std::array<T, Column> (&arr)[Row]) {
            std::array<std::array<T, Row>, Column> cp;
            std::copy(std::begin(arr), std::end(arr), cp.begin());
            return {cp};
        }


        template<typename T, size_t Row, size_t Column>
        constexpr Mat<T, Row, Column> make(std::initializer_list<std::initializer_list<T>> init) {
            std::array<std::array<T, Row>, Column> cp;
            auto itr1 = init.begin();
            for (size_t i = 0; init.end() != itr1 && i < Row; i++) {
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
        requires (R::type().row == R::type().column)
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
        Mat4x4<R> make_rotation(angle::Radian rad, _Vector<R, CI> axis) {
            static_assert(std::same_as<CI, VSL_NAMESPACE::D3>); // oto requires
            axis.normalize(); // 安全のため正規化
            R c = cos(rad.value);
            R s = sin(rad.value);
            R one_c = 1.0f - c;

            R x = axis.value.x.value;
            R y = axis.value.y.value;
            R z = axis.value.z.value;

            return make({
                                std::array<R, 4>{c + x * x * one_c, x * y * one_c - z * s, x * z * one_c + y * s, 0.0f},
                                std::array<R, 4>{y * x * one_c + z * s, c + y * y * one_c, y * z * one_c - x * s, 0.0f},
                                std::array<R, 4>{z * x * one_c - y * s, z * y * one_c + x * s, c + z * z * one_c, 0.0f},
                                std::array<R, 4>{0.0f, 0.0f, 0.0f, 1.0f}
                        });
        }

        template<typename R, typename CI>
        requires std::same_as<typename CI::dimension, VSL_NAMESPACE::D3>
        Mat4x4<R> make_rotation(angle::Degree deg, _Vector<R, CI> axis) {
            return make_rotation(Radian(deg), axis);
        }

        template<typename R, typename D, typename CI>
        Mat4x4<R> make_view(_Point<R, D, CI> _eye, _Point<R, D, CI> target, _Vector<R, D, CI> up) {
            _Vector<R, D, CI> eye(_eye.value);
            auto f = _Vector<R, D, CI>((-target).value - eye.value).normalize().value; // forward
            auto s = _Vector<R, D, CI>(f).cross(-up).normalize().value; // right
            auto u = _Vector<R, D, CI>(s).cross(_Vector<R, D, CI>(f)).normalize().value;             // up (recomputed)

            Mat4x4<R> result = make_identity<float, 4>();
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

            return make({
                                std::array<R, 4>{f / aspect, 0.0f, 0.0f, 0.0f},
                                std::array<R, 4>{0.0f, f, 0.0f, 0.0f},
                                std::array<R, 4>{0.0f, 0.0f, (far + near) * nf, -1.0f},
                                std::array<R, 4>{0.0f, 0.0f, (2 * far * near) * nf, 0.0f}
                        });

        }

        template<typename T>
        Mat4x4<T> make_perspective(Degree fov, T aspect, T near, T far) {
            return make_perspective((Radian) fov, aspect, near, far);
        }

        template<typename R, size_t Row, size_t Column>
        std::ostream &operator<<(std::ostream &out, const Mat<R, Row, Column> &mat) {
            out << "{";
            for (size_t i = 0; i < Column; i++) {
                out << "{" << mat[i][0];
                for (size_t j = 1; j < Row; j++)
                    out << ", " << mat[j][i];
                out << (i - 1 == Row ? "}}" : "}") << std::endl;
            }
            return out;
        };
    }
}