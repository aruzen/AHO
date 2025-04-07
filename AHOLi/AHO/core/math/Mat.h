#pragma once
#include "../../define.h"

#include <array>
#include <algorithm> 
#include <optional>

#include "../../concepts.h"
#include <VSL/dimention.h>

namespace AHO_NAMESPACE {
	namespace matrix {
		template<typename R, size_t Row, size_t Column>
		struct Mat;

		struct Type {
			size_t row, column;
		};

		template<typename T>
		concept to_matrixable = requires(T & t) {
			typename T::element_type;
			{ t.type() } -> std::same_as<matrix::Type>;
			{ t.to_matrix() } -> std::same_as<Mat<typename T::element_type, T::type().row, T::type().colomn>>;
		};

		template<typename T>
		concept is_matrix = requires(T & t) {
			to_matrixable<T>;
			t + std::declval<Mat<typename T::element_type, T::type().row, T::type().colomn>>();
			t - std::declval<Mat<typename T::element_type, T::type().row, T::type().colomn>>();
			t* std::declval<Mat<typename T::element_type, T::type().row, T::type().colomn>>();
			t += std::declval<Mat<typename T::element_type, T::type().row, T::type().colomn>>();
			t -= std::declval<Mat<typename T::element_type, T::type().row, T::type().colomn>>();
			t *= std::declval<Mat<typename T::element_type, T::type().row, T::type().colomn>>();
		};

		template<typename R, size_t Row, size_t Column>
		struct Mat : public std::array<std::array<R, Column>, Row> {
			using element_type = R;

			using RowType = std::array<R, Column>;

			static consteval Type type() {
				return { Row, Column };
			}

			constexpr Mat() = default;
			constexpr Mat(const std::array<std::array<R, Column>, Row>& init);

			constexpr AHO_NAMESPACE::matrix::Mat<R, Row, Column> operator +() const;

			constexpr AHO_NAMESPACE::matrix::Mat<R, Row, Column> operator -() const;

			template<AHO_NAMESPACE::concepts::add_as<R> T>
			constexpr auto operator +(const AHO_NAMESPACE::matrix::Mat<T, Row, Column>& p) const;

			template<AHO_NAMESPACE::concepts::subtract_as<R> T>
			constexpr auto operator -(const AHO_NAMESPACE::matrix::Mat<T, Row, Column>& p) const;

			template<AHO_NAMESPACE::concepts::multiply_as<R> T, size_t TargetColumn>
			constexpr auto operator *(const AHO_NAMESPACE::matrix::Mat<R, Column, TargetColumn>& p) const;

			template<AHO_NAMESPACE::concepts::multiply_as<R> T>
			constexpr auto operator *(const T& m) const;

			template<AHO_NAMESPACE::concepts::division_as<R> T>
			constexpr auto operator /(const T& m) const;

			template<AHO_NAMESPACE::concepts::add_as<R> T>
			constexpr Mat<R, Row, Column>& operator +=(const AHO_NAMESPACE::matrix::Mat<T, Row, Column>& p);

			template<AHO_NAMESPACE::concepts::subtract_as<R> T>
			constexpr Mat<R, Row, Column>& operator -=(const AHO_NAMESPACE::matrix::Mat<T, Row, Column>& p);

			template<AHO_NAMESPACE::concepts::multiply_as<R> T>
			constexpr Mat<R, Row, Column>& operator *=(const AHO_NAMESPACE::matrix::Mat<R, Row, Column>& p);

			template<AHO_NAMESPACE::concepts::multiply_as<R> T>
			constexpr Mat<R, Row, Column> operator *=(const T& m);

			template<AHO_NAMESPACE::concepts::division_as<R> T>
			constexpr Mat<R, Row, Column> operator /=(const T& m);

			template<AHO_NAMESPACE::concepts::cast_as<R> T>
			Mat<T, Row, Column> cast();
		};

		template <typename T, size_t N1>
		std::array<T, N1> row(const T(&arr)[N1]) {
			std::array<T, N1> cp;
			std::copy(std::begin(arr), std::end(arr), cp.begin());
			return cp;
		}

		template <typename T, size_t Row, size_t Column>
		constexpr Mat<T, Row, Column> make(const std::array<T, Column> (&arr)[Row]) {
			std::array<std::array<T, Row>, Column> cp;
			std::copy(std::begin(arr), std::end(arr), cp.begin());
			return { cp };
		}

		template<typename T, VSL_NAMESPACE::is_dimention D> 
		constexpr auto make_rotateX() {

		}
	}

	template<typename R, size_t Row, size_t Column>
	using Mat = matrix::Mat<R, Row, Column>;

	template<typename R>
	using Mat2x2 = matrix::Mat<R, 2, 2>;

	template<typename R>
	using Mat4x4 = matrix::Mat<R, 4, 4>;
	//------------------------------------------------------------------------------------------

	namespace matrix {
		template<typename R, size_t Row, size_t Column>
		constexpr Mat<R, Row, Column>::Mat(const std::array<std::array<R, Column>, Row>& init) : std::array<std::array<R, Column>, Row>{ init } {}

		template<typename R, size_t Row, size_t Column>
		constexpr AHO_NAMESPACE::matrix::Mat<R, Row, Column> Mat<R, Row, Column>::operator +() const {
			return *this;
		}

		template<typename R, size_t Row, size_t Column>
		constexpr AHO_NAMESPACE::matrix::Mat<R, Row, Column> Mat<R, Row, Column>::operator -() const {
			Mat<R, Row, Column> buf;
			for (size_t i = 0; i < Row; i++)
				for (size_t j = 0; j < Column; j++)
					buf[i][j] = -(*this)[i][j];
			return std::move(buf);
		}

		template<typename R, size_t Row, size_t Column>
		template<AHO_NAMESPACE::concepts::add_as<R> T>
		constexpr auto Mat<R, Row, Column>::operator +(const Mat<T, Row, Column>& p) const {
			Mat<decltype(std::declval<R>() + std::declval<T>()), Row, Column> buf;
			for (size_t i = 0; i < Row; i++)
				for (size_t j = 0; j < Column; j++)
					buf[i][j] = (*this)[i][j] + p[i][j];
			return std::move(buf);
		}

		template<typename R, size_t Row, size_t Column>
		template<AHO_NAMESPACE::concepts::subtract_as<R> T>
		constexpr auto Mat<R, Row, Column>::operator -(const Mat<T, Row, Column>& p) const {
			Mat<decltype(std::declval<R>() - std::declval<T>()), Row, Column> buf;
			for (size_t i = 0; i < Row; i++)
				for (size_t j = 0; j < Column; j++)
					buf[i][j] = (*this)[i][j] - p[i][j];
			return std::move(buf);
		}

		template<typename R, size_t Row, size_t Column>
		template<AHO_NAMESPACE::concepts::multiply_as<R> T, size_t TargetColumn>
		constexpr auto Mat<R, Row, Column>::operator *(const AHO_NAMESPACE::matrix::Mat<R, Column, TargetColumn>& p) const {
			AHO_NAMESPACE::matrix::Mat<decltype(std::declval<T>() * std::declval<R>()), Row, TargetColumn> buf;

			for (size_t i = 0; i < Row; i++)
				for (size_t j = 0; j < TargetColumn; j++)
					for (size_t k = 0; k < Column; k++)
						buf[i][j] = (*this)[i][k] + p[k][j];
			return std::move(buf);
		}

		template<typename R, size_t Row, size_t Column>
		template<AHO_NAMESPACE::concepts::multiply_as<R> T>
		constexpr auto Mat<R, Row, Column>::operator *(const T& m) const {
			AHO_NAMESPACE::matrix::Mat<decltype(std::declval<T>() * std::declval<R>()), Row, Column> buf;

			for (size_t i = 0; i < Row; i++)
				for (size_t j = 0; j < Column; j++)
					buf[i][j] = (*this)[i][j] * m;
			return std::move(buf);
		}

		template<typename R, size_t Row, size_t Column>
		template<AHO_NAMESPACE::concepts::division_as<R> T>
		constexpr auto Mat<R, Row, Column>::operator /(const T& m) const {
			AHO_NAMESPACE::matrix::Mat< decltype(std::declval<T>() / std::declval<R>()), Row, Column> buf;

			for (size_t i = 0; i < Row; i++)
				for (size_t j = 0; j < Column; j++)
					buf[i][j] = (*this)[i][j] / m;
			return std::move(buf);
		}

		template<typename R, size_t Row, size_t Column>
		template<AHO_NAMESPACE::concepts::add_as<R> T>
		constexpr Mat<R, Row, Column>& Mat<R, Row, Column>::operator +=(const AHO_NAMESPACE::matrix::Mat<T, Row, Column>& p) {
			for (size_t i = 0; i < Row; i++)
				for (size_t j = 0; j < Column; j++)
					(*this)[i][j] += p[i][j];
			return *this;
		}

		template<typename R, size_t Row, size_t Column>
		template<AHO_NAMESPACE::concepts::subtract_as<R> T>
		constexpr Mat<R, Row, Column>& Mat<R, Row, Column>::operator -=(const AHO_NAMESPACE::matrix::Mat<T, Row, Column>& p) {
			for (size_t i = 0; i < Row; i++)
				for (size_t j = 0; j < Column; j++)
					(*this)[i][j] + -p[i][j];
			return *this;
		}

		template<typename R, size_t Row, size_t Column>
		template<AHO_NAMESPACE::concepts::multiply_as<R> T>
		constexpr Mat<R, Row, Column>& Mat<R, Row, Column>::operator *=(const AHO_NAMESPACE::matrix::Mat<R, Row, Column>& p) {
			// static_assert(Row == Column, "different type  (*= -> *)");

			for (size_t i = 0; i < Row; i++)
				for (size_t j = 0; j < Column; j++)
					for (size_t k = 0; k < Column; k++)
						(*this)[i][k] += p[k][j];
			return *this;
		}

		template<typename R, size_t Row, size_t Column>
		template<AHO_NAMESPACE::concepts::multiply_as<R> T>
		constexpr Mat<R, Row, Column> Mat<R, Row, Column>::operator *=(const T& m) {
			for (size_t i = 0; i < Row; i++)
				for (size_t j = 0; j < Column; j++)
					(*this)[i][j] *= m;
			return *this;
		}

		template<typename R, size_t Row, size_t Column>
		template<AHO_NAMESPACE::concepts::division_as<R> T>
		constexpr Mat<R, Row, Column> Mat<R, Row, Column>::operator /=(const T& m) {
			for (size_t i = 0; i < Row; i++)
				for (size_t j = 0; j < Column; j++)
					(*this)[i][j] /= m;
			return *this;
		}

		template<typename R, size_t Row, size_t Column>
		template<AHO_NAMESPACE::concepts::cast_as<R> T>
		Mat<T, Row, Column>  Mat<R, Row, Column>::cast() {
			AHO_NAMESPACE::matrix::Mat<T, Row, Column> buf;
			for (size_t i = 0; i < Row; i++)
				for (size_t j = 0; j < Column; j++)
					buf[i][j] = (T)(*this)[i][j];
			return std::move(buf);
		}
	}
}