#pragma once
#include "define.h"

#include <cstdint>
#include <concepts>

namespace VSL_NAMESPACE::data_format {
	enum class ___FormatTypes : std::uint32_t{
		Undefined = 0,
		UnsignedNormalized8R = 9,
		SignedNormalized8R = 10,
		UnsignedScaled8R = 11,
		SignedScaled8R = 12,
		UnsignedInt8R = 13,
		SignedInt8R = 14,
		Srgb8R = 15,
		UnsignedNormalized8RG = 16,
		SignedNormalized8RG = 17,
		UnsignedScaled8RG = 18,
		SignedScaled8RG = 19,
		UnsignedInt8RG = 20,
		SignedInt8RG = 21,
		Srgb8RG = 22,
		UnsignedNormalized8RGB = 23,
		SignedNormalized8RGB = 24,
		UnsignedScaled8RGB = 25,
		SignedScaled8RGB = 26,
		UnsignedInt8RGB = 27,
		SignedInt8RGB = 28,
		Srgb8RGB = 29,
		UnsignedNormalized8BGR = 30,
		SignedNormalized8BGR = 31,
		UnsignedScaled8BGR = 32,
		SignedScaled8BGR = 33,
		UnsignedInt8BGR = 34,
		SignedInt8BGR = 35,
		Srgb8BGR = 36,
		UnsignedNormalized8RGBA = 37,
		SignedNormalized8RGBA = 38,
		UnsignedScaled8RGBA = 39,
		SignedScaled8RGBA = 40,
		UnsignedInt8RGBA = 41,
		SignedInt8RGBA = 42,
		Srgb8RGBA = 43,
		UnsignedNormalized8BGRA = 44,
		SignedNormalized8BGRA = 45,
		UnsignedScaled8BGRA = 46,
		SignedScaled8BGRA = 47,
		UnsignedInt8BGRA = 48,
		SignedInt8BGRA = 49,
		Srgb8BGRA = 50,
		UnsignedNormalized8ABGRPACK32 = 51,
		SignedNormalized8ABGRPACK32 = 52,
		UnsignedScaled8ABGRPACK32 = 53,
		SignedScaled8ABGRPACK32 = 54,
		UnsignedInt8ABGRPACK32 = 55,
		SignedInt8ABGRPACK32 = 56,
		Srgb8ABGRPACK32 = 57,
		UnsignedNormalized16R = 70,
		SignedNormalized16R = 71,
		UnsignedScaled16R = 72,
		SignedScaled16R = 73,
		UnsignedInt16R = 74,
		SignedInt16R = 75,
		SignedFloat16R = 76,
		UnsignedNormalized16RG = 77,
		SignedNormalized16RG = 78,
		UnsignedScaled16RG = 79,
		SignedScaled16RG = 80,
		UnsignedInt16RG = 81,
		SignedInt16RG = 82,
		SignedFloat16RG = 83,
		UnsignedNormalized16RGB = 84,
		SignedNormalized16RGB = 85,
		UnsignedScaled16RGB = 86,
		SignedScaled16RGB = 87,
		UnsignedInt16RGB = 88,
		SignedInt16RGB = 89,
		SignedFloat16RGB = 90,
		UnsignedNormalized16RGBA = 91,
		SignedNormalized16RGBA = 92,
		UnsignedScaled16RGBA = 93,
		SignedScaled16RGBA = 94,
		UnsignedInt16RGBA = 95,
		SignedInt16RGBA = 96,
		SignedFloat16RGBA = 97,
		UnsignedInt32R = 98,
		SignedInt32R = 99,
		SignedFloat32R = 100,
		UnsignedInt32RG = 101,
		SignedInt32RG = 102,
		SignedFloat32RG = 103,
		UnsignedInt32RGB = 104,
		SignedInt32RGB = 105,
		SignedFloat32RGB = 106,
		UnsignedInt32RGBA = 107,
		SignedInt32RGBA = 108,
		SignedFloat32RGBA = 109,
		UnsignedInt64R = 110,
		SignedInt64R = 111,
		SignedFloat64R = 112,
		UnsignedInt64RG = 113,
		SignedInt64RG = 114,
		SignedFloat64RG = 115,
		UnsignedInt64RGB = 116,
		SignedInt64RGB = 117,
		SignedFloat64RGB = 118,
		UnsignedInt64RGBA = 119,
		SignedInt64RGBA = 120,
		SignedFloat64RGBA = 121,


		UnsignedNormalized8Vec1 = UnsignedNormalized8R,
		SignedNormalized8Vec1 = SignedNormalized8R,
		UnsignedScaled8Vec1 = UnsignedScaled8R,
		SignedScaled8Vec1 = SignedScaled8R,
		UnsignedInt8Vec1 = UnsignedInt8R,
		SignedInt8Vec1 = SignedInt8R,
		Srgb8Vec1 = Srgb8R,
		UnsignedNormalized8Vec2 = UnsignedNormalized8RG,
		SignedNormalized8Vec2 = SignedNormalized8RG,
		UnsignedScaled8Vec2 = UnsignedScaled8RG,
		SignedScaled8Vec2 = SignedScaled8RG,
		UnsignedInt8Vec2 = UnsignedInt8RG,
		SignedInt8Vec2 = SignedInt8RG,
		Srgb8Vec2 = Srgb8RG,
		UnsignedNormalized8Vec3 = UnsignedNormalized8RGB,
		SignedNormalized8Vec3 = SignedNormalized8RGB,
		UnsignedScaled8Vec3 = UnsignedScaled8RGB,
		SignedScaled8Vec3 = SignedScaled8RGB,
		UnsignedInt8Vec3 = UnsignedInt8RGB,
		SignedInt8Vec3 = SignedInt8RGB,
		Srgb8Vec3 = Srgb8RGB,
		UnsignedNormalized8Vec4 = UnsignedNormalized8RGBA,
		SignedNormalized8Vec4 = SignedNormalized8RGBA,
		UnsignedScaled8Vec4 = UnsignedScaled8RGBA,
		SignedScaled8Vec4 = SignedScaled8RGBA,
		UnsignedInt8Vec4 = UnsignedInt8RGBA,
		SignedInt8Vec4 = SignedInt8RGBA,
		Srgb8Vec4 = Srgb8RGBA,
		UnsignedNormalized16Vec1 = UnsignedNormalized16R,
		SignedNormalized16Vec1 = SignedNormalized16R,
		UnsignedScaled16Vec1 = UnsignedScaled16R,
		SignedScaled16Vec1 = SignedScaled16R,
		UnsignedInt16Vec1 = UnsignedInt16R,
		SignedInt16Vec1 = SignedInt16R,
		SignedFloat16Vec1 = SignedFloat16R,
		UnsignedNormalized16Vec2 = UnsignedNormalized16RG,
		SignedNormalized16Vec2 = SignedNormalized16RG,
		UnsignedScaled16Vec2 = UnsignedScaled16RG,
		SignedScaled16Vec2 = SignedScaled16RG,
		UnsignedInt16Vec2 = UnsignedInt16RG,
		SignedInt16Vec2 = SignedInt16RG,
		SignedFloat16Vec2 = SignedFloat16RG,
		UnsignedNormalized16Vec3 = UnsignedNormalized16RGB,
		SignedNormalized16Vec3 = SignedNormalized16RGB,
		UnsignedScaled16Vec3 = UnsignedScaled16RGB,
		SignedScaled16Vec3 = SignedScaled16RGB,
		UnsignedInt16Vec3 = UnsignedInt16RGB,
		SignedInt16Vec3 = SignedInt16RGB,
		SignedFloat16Vec3 = SignedFloat16RGB,
		UnsignedNormalized16Vec4 = UnsignedNormalized16RGBA,
		SignedNormalized16Vec4 = SignedNormalized16RGBA,
		UnsignedScaled16Vec4 = UnsignedScaled16RGBA,
		SignedScaled16Vec4 = SignedScaled16RGBA,
		UnsignedInt16Vec4 = UnsignedInt16RGBA,
		SignedInt16Vec4 = SignedInt16RGBA,
		SignedFloat16Vec4 = SignedFloat16RGBA,
		UnsignedInt32Vec1 = UnsignedInt32R,
		SignedInt32Vec1 = SignedInt32R,
		SignedFloat32Vec1 = SignedFloat32R,
		UnsignedInt32Vec2 = UnsignedInt32RG,
		SignedInt32Vec2 = SignedInt32RG,
		SignedFloat32Vec2 = SignedFloat32RG,
		UnsignedInt32Vec3 = UnsignedInt32RGB,
		SignedInt32Vec3 = SignedInt32RGB,
		SignedFloat32Vec3 = SignedFloat32RGB,
		UnsignedInt32Vec4 = UnsignedInt32RGBA,
		SignedInt32Vec4 = SignedInt32RGBA,
		SignedFloat32Vec4 = SignedFloat32RGBA,
		UnsignedInt64Vec1 = UnsignedInt64R,
		SignedInt64Vec1 = SignedInt64R,
		SignedFloat64Vec1 = SignedFloat64R,
		UnsignedInt64Vec2 = UnsignedInt64RG,
		SignedInt64Vec2 = SignedInt64RG,
		SignedFloat64Vec2 = SignedFloat64RG,
		UnsignedInt64Vec3 = UnsignedInt64RGB,
		SignedInt64Vec3 = SignedInt64RGB,
		SignedFloat64Vec3 = SignedFloat64RGB,
		UnsignedInt64Vec4 = UnsignedInt64RGBA,
		SignedInt64Vec4 = SignedInt64RGBA,
		SignedFloat64Vec4 = SignedFloat64RGBA,

		Normalized8R = SignedNormalized8R,
		Scaled8R = SignedScaled8R,
		Int8R = SignedInt8R,
		Normalized8RG = SignedNormalized8RG,
		Scaled8RG = SignedScaled8RG,
		Int8RG = SignedInt8RG,
		Normalized8RGB = SignedNormalized8RGB,
		Scaled8RGB = SignedScaled8RGB,
		Int8RGB = SignedInt8RGB,
		Normalized8BGR = SignedNormalized8BGR,
		Scaled8BGR = SignedScaled8BGR,
		Int8BGR = SignedInt8BGR,
		Normalized8RGBA = SignedNormalized8RGBA,
		Scaled8RGBA = SignedScaled8RGBA,
		Int8RGBA = SignedInt8RGBA,
		Normalized8BGRA = SignedNormalized8BGRA,
		Scaled8BGRA = SignedScaled8BGRA,
		Int8BGRA = SignedInt8BGRA,
		Normalized8ABGRPACK32 = SignedNormalized8ABGRPACK32,
		Scaled8ABGRPACK32 = SignedScaled8ABGRPACK32,
		Int8ABGRPACK32 = SignedInt8ABGRPACK32,
		Normalized16R = SignedNormalized16R,
		Scaled16R = SignedScaled16R,
		Int16R = SignedInt16R,
		Float16R = SignedFloat16R,
		Normalized16RG = SignedNormalized16RG,
		Scaled16RG = SignedScaled16RG,
		Int16RG = SignedInt16RG,
		Float16RG = SignedFloat16RG,
		Normalized16RGB = SignedNormalized16RGB,
		Scaled16RGB = SignedScaled16RGB,
		Int16RGB = SignedInt16RGB,
		Float16RGB = SignedFloat16RGB,
		Normalized16RGBA = SignedNormalized16RGBA,
		Scaled16RGBA = SignedScaled16RGBA,
		Int16RGBA = SignedInt16RGBA,
		Float16RGBA = SignedFloat16RGBA,
		Int32R = SignedInt32R,
		Float32R = SignedFloat32R,
		Int32RG = SignedInt32RG,
		Float32RG = SignedFloat32RG,
		Int32RGB = SignedInt32RGB,
		Float32RGB = SignedFloat32RGB,
		Int32RGBA = SignedInt32RGBA,
		Float32RGBA = SignedFloat32RGBA,
		Int64R = SignedInt64R,
		Float64R = SignedFloat64R,
		Int64RG = SignedInt64RG,
		Float64RG = SignedFloat64RG,
		Int64RGB = SignedInt64RGB,
		Float64RGB = SignedFloat64RGB,
		Int64RGBA = SignedInt64RGBA,
		Float64RGBA = SignedFloat64RGBA,
		Normalized8Vec1 = SignedNormalized8Vec1,
		Scaled8Vec1 = SignedScaled8Vec1,
		Int8Vec1 = SignedInt8Vec1,
		Normalized8Vec2 = SignedNormalized8Vec2,
		Scaled8Vec2 = SignedScaled8Vec2,
		Int8Vec2 = SignedInt8Vec2,
		Normalized8Vec3 = SignedNormalized8Vec3,
		Scaled8Vec3 = SignedScaled8Vec3,
		Int8Vec3 = SignedInt8Vec3,
		Normalized8Vec4 = SignedNormalized8Vec4,
		Scaled8Vec4 = SignedScaled8Vec4,
		Int8Vec4 = SignedInt8Vec4,
		Normalized16Vec1 = SignedNormalized16Vec1,
		Scaled16Vec1 = SignedScaled16Vec1,
		Int16Vec1 = SignedInt16Vec1,
		Float16Vec1 = SignedFloat16Vec1,
		Normalized16Vec2 = SignedNormalized16Vec2,
		Scaled16Vec2 = SignedScaled16Vec2,
		Int16Vec2 = SignedInt16Vec2,
		Float16Vec2 = SignedFloat16Vec2,
		Normalized16Vec3 = SignedNormalized16Vec3,
		Scaled16Vec3 = SignedScaled16Vec3,
		Int16Vec3 = SignedInt16Vec3,
		Float16Vec3 = SignedFloat16Vec3,
		Normalized16Vec4 = SignedNormalized16Vec4,
		Scaled16Vec4 = SignedScaled16Vec4,
		Int16Vec4 = SignedInt16Vec4,
		Float16Vec4 = SignedFloat16Vec4,
		Int32Vec1 = SignedInt32Vec1,
		Float32Vec1 = SignedFloat32Vec1,
		Int32Vec2 = SignedInt32Vec2,
		Float32Vec2 = SignedFloat32Vec2,
		Int32Vec3 = SignedInt32Vec3,
		Float32Vec3 = SignedFloat32Vec3,
		Int32Vec4 = SignedInt32Vec4,
		Float32Vec4 = SignedFloat32Vec4,
		Int64Vec1 = SignedInt64Vec1,
		Float64Vec1 = SignedFloat64Vec1,
		Int64Vec2 = SignedInt64Vec2,
		Float64Vec2 = SignedFloat64Vec2,
		Int64Vec3 = SignedInt64Vec3,
		Float64Vec3 = SignedFloat64Vec3,
		Int64Vec4 = SignedInt64Vec4,
		Float64Vec4 = SignedFloat64Vec4,

		UnsignedNormalized8 = UnsignedNormalized8R,
		SignedNormalized8 = SignedNormalized8R,
		UnsignedScaled8 = UnsignedScaled8R,
		SignedScaled8 = SignedScaled8R,
		UnsignedInt8 = UnsignedInt8R,
		SignedInt8 = SignedInt8R,
		Srgb8 = Srgb8R,
		UnsignedNormalized16 = UnsignedNormalized16R,
		SignedNormalized16 = SignedNormalized16R,
		UnsignedScaled16 = UnsignedScaled16R,
		SignedScaled16 = SignedScaled16R,
		UnsignedInt16 = UnsignedInt16R,
		SignedInt16 = SignedInt16R,
		SignedFloat16 = SignedFloat16R,
		UnsignedInt32 = UnsignedInt32R,
		SignedInt32 = SignedInt32R,
		SignedFloat32 = SignedFloat32R,
		UnsignedInt64 = UnsignedInt64R,
		SignedInt64 = SignedInt64R,
		SignedFloat64 = SignedFloat64R,
		Normalized8 = SignedNormalized8Vec1,
		Scaled8 = SignedScaled8Vec1,
		Int8 = SignedInt8Vec1,
		Normalized16 = SignedNormalized16Vec1,
		Scaled16 = SignedScaled16Vec1,
		Int16 = SignedInt16Vec1,
		Float16 = SignedFloat16Vec1,
		Int32 = SignedInt32Vec1,
		Float32 = SignedFloat32Vec1,
		Int64 = SignedInt64Vec1,
		Float64 = SignedFloat64Vec1,

		UnsignedIntR = UnsignedInt32R,
		SignedIntR = SignedInt32R,
		SignedFloatR = SignedFloat32R,
		UnsignedIntRG = UnsignedInt32RG,
		SignedIntRG = SignedInt32RG,
		SignedFloatRG = SignedFloat32RG,
		UnsignedIntRGB = UnsignedInt32RGB,
		SignedIntRGB = SignedInt32RGB,
		SignedFloatRGB = SignedFloat32RGB,
		UnsignedIntRGBA = UnsignedInt32RGBA,
		SignedIntRGBA = SignedInt32RGBA,
		SignedFloatRGBA = SignedFloat32RGBA,
		UnsignedIntVec1 = UnsignedInt32R,
		SignedIntVec1 = SignedInt32R,
		SignedFloatVec1 = SignedFloat32R,
		UnsignedIntVec2 = UnsignedInt32RG,
		SignedIntVec2 = SignedInt32RG,
		SignedFloatVec2 = SignedFloat32RG,
		UnsignedIntVec3 = UnsignedInt32RGB,
		SignedIntVec3 = SignedInt32RGB,
		SignedFloatVec3 = SignedFloat32RGB,
		UnsignedIntVec4 = UnsignedInt32RGBA,
		SignedIntVec4 = SignedInt32RGBA,
		SignedFloatVec4 = SignedFloat32RGBA,
		IntR = SignedInt32R,
		FloatR = SignedFloat32R,
		IntRG = SignedInt32RG,
		FloatRG = SignedFloat32RG,
		IntRGB = SignedInt32RGB,
		FloatRGB = SignedFloat32RGB,
		IntRGBA = SignedInt32RGBA,
		FloatRGBA = SignedFloat32RGBA,
		IntVec1 = SignedInt32Vec1,
		FloatVec1 = SignedFloat32Vec1,
		IntVec2 = SignedInt32Vec2,
		FloatVec2 = SignedFloat32Vec2,
		IntVec3 = SignedInt32Vec3,
		FloatVec3 = SignedFloat32Vec3,
		IntVec4 = SignedInt32Vec4,
		FloatVec4 = SignedFloat32Vec4,
		UnsignedInt = UnsignedInt32R,
		SignedInt = SignedInt32R,
		SignedFloat = SignedFloat32R,
		Int = SignedInt32Vec1,
		Float = SignedFloat32Vec1,
	};

	class ___Format {
		___FormatTypes _formatType = ___FormatTypes::Undefined;
		unsigned int _size = 0;
	public:
		constexpr ___Format(___FormatTypes formatType, std::uint32_t size) : _formatType(formatType), _size(size) {}

		constexpr ___FormatTypes type() const {
			return _formatType;
		}

		constexpr std::uint32_t size() const {
			return _size;
		}

		constexpr size_t getVecLength() const;

		template <size_t Length>
		constexpr ___Format toVec()  const;

		constexpr auto operator <=>(const ___Format&) const = default;
	};

	constexpr ___Format Undefined(___FormatTypes::Undefined, 0);

	constexpr ___Format UnsignedNormalized8R(___FormatTypes::UnsignedNormalized8R, 1 * 1);
	constexpr ___Format SignedNormalized8R(___FormatTypes::SignedNormalized8R, 1 * 1);
	constexpr ___Format UnsignedScaled8R(___FormatTypes::UnsignedScaled8R, 1 * 1);
	constexpr ___Format SignedScaled8R(___FormatTypes::SignedScaled8R, 1 * 1);
	constexpr ___Format UnsignedInt8R(___FormatTypes::UnsignedInt8R, 1 * 1);
	constexpr ___Format SignedInt8R(___FormatTypes::SignedInt8R, 1 * 1);
	constexpr ___Format Srgb8R(___FormatTypes::Srgb8R, 1 * 1);
	constexpr ___Format UnsignedNormalized8RG(___FormatTypes::UnsignedNormalized8RG, 2 * 1);
	constexpr ___Format SignedNormalized8RG(___FormatTypes::SignedNormalized8RG, 2 * 1);
	constexpr ___Format UnsignedScaled8RG(___FormatTypes::UnsignedScaled8RG, 2 * 1);
	constexpr ___Format SignedScaled8RG(___FormatTypes::SignedScaled8RG, 2 * 1);
	constexpr ___Format UnsignedInt8RG(___FormatTypes::UnsignedInt8RG, 2 * 1);
	constexpr ___Format SignedInt8RG(___FormatTypes::SignedInt8RG, 2 * 1);
	constexpr ___Format Srgb8RG(___FormatTypes::Srgb8RG, 2 * 1);
	constexpr ___Format UnsignedNormalized8RGB(___FormatTypes::UnsignedNormalized8RGB, 3 * 1);
	constexpr ___Format SignedNormalized8RGB(___FormatTypes::SignedNormalized8RGB, 3 * 3 * 1);
	constexpr ___Format UnsignedScaled8RGB(___FormatTypes::UnsignedScaled8RGB, 3 * 1);
	constexpr ___Format SignedScaled8RGB(___FormatTypes::SignedScaled8RGB, 3 * 1);
	constexpr ___Format UnsignedInt8RGB(___FormatTypes::UnsignedInt8RGB, 3 * 1);
	constexpr ___Format SignedInt8RGB(___FormatTypes::SignedInt8RGB, 3 * 1);
	constexpr ___Format Srgb8RGB(___FormatTypes::Srgb8RGB, 3 * 1);
	constexpr ___Format UnsignedNormalized8BGR(___FormatTypes::UnsignedNormalized8BGR, 3 * 1);
	constexpr ___Format SignedNormalized8BGR(___FormatTypes::SignedNormalized8BGR, 3 * 1);
	constexpr ___Format UnsignedScaled8BGR(___FormatTypes::UnsignedScaled8BGR, 3 * 1);
	constexpr ___Format SignedScaled8BGR(___FormatTypes::SignedScaled8BGR, 3 * 1);
	constexpr ___Format UnsignedInt8BGR(___FormatTypes::UnsignedInt8BGR, 3 * 1);
	constexpr ___Format SignedInt8BGR(___FormatTypes::SignedInt8BGR, 3 * 1);
	constexpr ___Format Srgb8BGR(___FormatTypes::Srgb8BGR, 3 * 1);
	constexpr ___Format UnsignedNormalized8RGBA(___FormatTypes::UnsignedNormalized8RGBA, 4 * 1);
	constexpr ___Format SignedNormalized8RGBA(___FormatTypes::SignedNormalized8RGBA, 4 * 1);
	constexpr ___Format UnsignedScaled8RGBA(___FormatTypes::UnsignedScaled8RGBA, 4 * 1);
	constexpr ___Format SignedScaled8RGBA(___FormatTypes::SignedScaled8RGBA, 4 * 1);
	constexpr ___Format UnsignedInt8RGBA(___FormatTypes::UnsignedInt8RGBA, 4 * 1);
	constexpr ___Format SignedInt8RGBA(___FormatTypes::SignedInt8RGBA, 4 * 1);
	constexpr ___Format Srgb8RGBA(___FormatTypes::Srgb8RGBA, 4 * 1);
	constexpr ___Format UnsignedNormalized8BGRA(___FormatTypes::UnsignedNormalized8BGRA, 4 * 1);
	constexpr ___Format SignedNormalized8BGRA(___FormatTypes::SignedNormalized8BGRA, 4 * 1);
	constexpr ___Format UnsignedScaled8BGRA(___FormatTypes::UnsignedScaled8BGRA, 4 * 1);
	constexpr ___Format SignedScaled8BGRA(___FormatTypes::SignedScaled8BGRA, 4 * 1);
	constexpr ___Format UnsignedInt8BGRA(___FormatTypes::UnsignedInt8BGRA, 4 * 1);
	constexpr ___Format SignedInt8BGRA(___FormatTypes::SignedInt8BGRA, 4 * 1);
	constexpr ___Format Srgb8BGRA(___FormatTypes::Srgb8BGRA, 4 * 1);
	constexpr ___Format UnsignedNormalized8ABGRPACK32(___FormatTypes::UnsignedNormalized8ABGRPACK32, 8);
	constexpr ___Format SignedNormalized8ABGRPACK32(___FormatTypes::SignedNormalized8ABGRPACK32, 4 * 1);
	constexpr ___Format UnsignedScaled8ABGRPACK32(___FormatTypes::UnsignedScaled8ABGRPACK32, 4 * 1);
	constexpr ___Format SignedScaled8ABGRPACK32(___FormatTypes::SignedScaled8ABGRPACK32, 4 * 1);
	constexpr ___Format UnsignedInt8ABGRPACK32(___FormatTypes::UnsignedInt8ABGRPACK32, 4 * 1);
	constexpr ___Format SignedInt8ABGRPACK32(___FormatTypes::SignedInt8ABGRPACK32, 4 * 1);
	constexpr ___Format Srgb8ABGRPACK32(___FormatTypes::Srgb8ABGRPACK32, 8);
	constexpr ___Format UnsignedNormalized16R(___FormatTypes::UnsignedNormalized16R, 1 * 2);
	constexpr ___Format SignedNormalized16R(___FormatTypes::SignedNormalized16R, 1 * 2);
	constexpr ___Format UnsignedScaled16R(___FormatTypes::UnsignedScaled16R, 1 * 2);
	constexpr ___Format SignedScaled16R(___FormatTypes::SignedScaled16R, 1 * 2);
	constexpr ___Format UnsignedInt16R(___FormatTypes::UnsignedInt16R, 1 * 2);
	constexpr ___Format SignedInt16R(___FormatTypes::SignedInt16R, 1 * 2);
	constexpr ___Format SignedFloat16R(___FormatTypes::SignedFloat16R, 1 * 2);
	constexpr ___Format UnsignedNormalized16RG(___FormatTypes::UnsignedNormalized16RG, 2 * 2);
	constexpr ___Format SignedNormalized16RG(___FormatTypes::SignedNormalized16RG, 2 * 2);
	constexpr ___Format UnsignedScaled16RG(___FormatTypes::UnsignedScaled16RG, 2 * 2);
	constexpr ___Format SignedScaled16RG(___FormatTypes::SignedScaled16RG, 2 * 2);
	constexpr ___Format UnsignedInt16RG(___FormatTypes::UnsignedInt16RG, 2 * 2);
	constexpr ___Format SignedInt16RG(___FormatTypes::SignedInt16RG, 2 * 2);
	constexpr ___Format SignedFloat16RG(___FormatTypes::SignedFloat16RG, 2 * 2);
	constexpr ___Format UnsignedNormalized16RGB(___FormatTypes::UnsignedNormalized16RGB, 3 * 2);
	constexpr ___Format SignedNormalized16RGB(___FormatTypes::SignedNormalized16RGB, 3 * 2);
	constexpr ___Format UnsignedScaled16RGB(___FormatTypes::UnsignedScaled16RGB, 3 * 2);
	constexpr ___Format SignedScaled16RGB(___FormatTypes::SignedScaled16RGB, 3 * 2);
	constexpr ___Format UnsignedInt16RGB(___FormatTypes::UnsignedInt16RGB, 3 * 2);
	constexpr ___Format SignedInt16RGB(___FormatTypes::SignedInt16RGB, 3 * 2);
	constexpr ___Format SignedFloat16RGB(___FormatTypes::SignedFloat16RGB, 3 * 2);
	constexpr ___Format UnsignedNormalized16RGBA(___FormatTypes::UnsignedNormalized16RGBA, 4 * 2);
	constexpr ___Format SignedNormalized16RGBA(___FormatTypes::SignedNormalized16RGBA, 4 * 2);
	constexpr ___Format UnsignedScaled16RGBA(___FormatTypes::UnsignedScaled16RGBA, 4 * 2);
	constexpr ___Format SignedScaled16RGBA(___FormatTypes::SignedScaled16RGBA, 4 * 2);
	constexpr ___Format UnsignedInt16RGBA(___FormatTypes::UnsignedInt16RGBA, 4 * 2);
	constexpr ___Format SignedInt16RGBA(___FormatTypes::SignedInt16RGBA, 4 * 2);
	constexpr ___Format SignedFloat16RGBA(___FormatTypes::SignedFloat16RGBA, 4 * 2);
	constexpr ___Format UnsignedInt32R(___FormatTypes::UnsignedInt32R, 1 * 4);
	constexpr ___Format SignedInt32R(___FormatTypes::SignedInt32R, 1 * 4);
	constexpr ___Format SignedFloat32R(___FormatTypes::SignedFloat32R, 1 * 4);
	constexpr ___Format UnsignedInt32RG(___FormatTypes::UnsignedInt32RG, 2 * 4);
	constexpr ___Format SignedInt32RG(___FormatTypes::SignedInt32RG, 2 * 4);
	constexpr ___Format SignedFloat32RG(___FormatTypes::SignedFloat32RG, 2 * 4);
	constexpr ___Format UnsignedInt32RGB(___FormatTypes::UnsignedInt32RGB, 3 * 4);
	constexpr ___Format SignedInt32RGB(___FormatTypes::SignedInt32RGB, 3 * 4);
	constexpr ___Format SignedFloat32RGB(___FormatTypes::SignedFloat32RGB, 3 * 4);
	constexpr ___Format UnsignedInt32RGBA(___FormatTypes::UnsignedInt32RGBA, 4 * 4);
	constexpr ___Format SignedInt32RGBA(___FormatTypes::SignedInt32RGBA, 4 * 4);
	constexpr ___Format SignedFloat32RGBA(___FormatTypes::SignedFloat32RGBA, 4 * 4);
	constexpr ___Format UnsignedInt64R(___FormatTypes::UnsignedInt64R, 1 * 8);
	constexpr ___Format SignedInt64R(___FormatTypes::SignedInt64R, 1 * 8);
	constexpr ___Format SignedFloat64R(___FormatTypes::SignedFloat64R, 1 * 8);
	constexpr ___Format UnsignedInt64RG(___FormatTypes::UnsignedInt64RG, 2 * 8);
	constexpr ___Format SignedInt64RG(___FormatTypes::SignedInt64RG, 2 * 8);
	constexpr ___Format SignedFloat64RG(___FormatTypes::SignedFloat64RG, 2 * 8);
	constexpr ___Format UnsignedInt64RGB(___FormatTypes::UnsignedInt64RGB, 3 * 8);
	constexpr ___Format SignedInt64RGB(___FormatTypes::SignedInt64RGB, 3 * 8);
	constexpr ___Format SignedFloat64RGB(___FormatTypes::SignedFloat64RGB, 3 * 8);
	constexpr ___Format UnsignedInt64RGBA(___FormatTypes::UnsignedInt64RGBA, 4 * 8);
	constexpr ___Format SignedInt64RGBA(___FormatTypes::SignedInt64RGBA, 4 * 8);
	constexpr ___Format SignedFloat64RGBA(___FormatTypes::SignedFloat64RGBA, 4 * 8);

	constexpr ___Format UnsignedNormalized8Vec1(___FormatTypes::UnsignedNormalized8Vec1, UnsignedNormalized8R.size());
	constexpr ___Format SignedNormalized8Vec1(___FormatTypes::SignedNormalized8Vec1, SignedNormalized8R.size());
	constexpr ___Format UnsignedScaled8Vec1(___FormatTypes::UnsignedScaled8Vec1, UnsignedScaled8R.size());
	constexpr ___Format SignedScaled8Vec1(___FormatTypes::SignedScaled8Vec1, SignedScaled8R.size());
	constexpr ___Format UnsignedInt8Vec1(___FormatTypes::UnsignedInt8Vec1, UnsignedInt8R.size());
	constexpr ___Format SignedInt8Vec1(___FormatTypes::SignedInt8Vec1, SignedInt8R.size());
	constexpr ___Format Srgb8Vec1(___FormatTypes::Srgb8Vec1, Srgb8R.size());
	constexpr ___Format UnsignedNormalized8Vec2(___FormatTypes::UnsignedNormalized8Vec2, UnsignedNormalized8RG.size());
	constexpr ___Format SignedNormalized8Vec2(___FormatTypes::SignedNormalized8Vec2, SignedNormalized8RG.size());
	constexpr ___Format UnsignedScaled8Vec2(___FormatTypes::UnsignedScaled8Vec2, UnsignedScaled8RG.size());
	constexpr ___Format SignedScaled8Vec2(___FormatTypes::SignedScaled8Vec2, SignedScaled8RG.size());
	constexpr ___Format UnsignedInt8Vec2(___FormatTypes::UnsignedInt8Vec2, UnsignedInt8RG.size());
	constexpr ___Format SignedInt8Vec2(___FormatTypes::SignedInt8Vec2, SignedInt8RG.size());
	constexpr ___Format Srgb8Vec2(___FormatTypes::Srgb8Vec2, Srgb8RG.size());
	constexpr ___Format UnsignedNormalized8Vec3(___FormatTypes::UnsignedNormalized8Vec3, UnsignedNormalized8RGB.size());
	constexpr ___Format SignedNormalized8Vec3(___FormatTypes::SignedNormalized8Vec3, SignedNormalized8RGB.size());
	constexpr ___Format UnsignedScaled8Vec3(___FormatTypes::UnsignedScaled8Vec3, UnsignedScaled8RGB.size());
	constexpr ___Format SignedScaled8Vec3(___FormatTypes::SignedScaled8Vec3, SignedScaled8RGB.size());
	constexpr ___Format UnsignedInt8Vec3(___FormatTypes::UnsignedInt8Vec3, UnsignedInt8RGB.size());
	constexpr ___Format SignedInt8Vec3(___FormatTypes::SignedInt8Vec3, SignedInt8RGB.size());
	constexpr ___Format Srgb8Vec3(___FormatTypes::Srgb8Vec3, Srgb8RGB.size());
	constexpr ___Format UnsignedNormalized8Vec4(___FormatTypes::UnsignedNormalized8Vec4, UnsignedNormalized8RGBA.size());
	constexpr ___Format SignedNormalized8Vec4(___FormatTypes::SignedNormalized8Vec4, SignedNormalized8RGBA.size());
	constexpr ___Format UnsignedScaled8Vec4(___FormatTypes::UnsignedScaled8Vec4, UnsignedScaled8RGBA.size());
	constexpr ___Format SignedScaled8Vec4(___FormatTypes::SignedScaled8Vec4, SignedScaled8RGBA.size());
	constexpr ___Format UnsignedInt8Vec4(___FormatTypes::UnsignedInt8Vec4, UnsignedInt8RGBA.size());
	constexpr ___Format SignedInt8Vec4(___FormatTypes::SignedInt8Vec4, SignedInt8RGBA.size());
	constexpr ___Format Srgb8Vec4(___FormatTypes::Srgb8Vec4, Srgb8RGBA.size());
	constexpr ___Format UnsignedNormalized16Vec1(___FormatTypes::UnsignedNormalized16Vec1, UnsignedNormalized16R.size());
	constexpr ___Format SignedNormalized16Vec1(___FormatTypes::SignedNormalized16Vec1, SignedNormalized16R.size());
	constexpr ___Format UnsignedScaled16Vec1(___FormatTypes::UnsignedScaled16Vec1, UnsignedScaled16R.size());
	constexpr ___Format SignedScaled16Vec1(___FormatTypes::SignedScaled16Vec1, SignedScaled16R.size());
	constexpr ___Format UnsignedInt16Vec1(___FormatTypes::UnsignedInt16Vec1, UnsignedInt16R.size());
	constexpr ___Format SignedInt16Vec1(___FormatTypes::SignedInt16Vec1, SignedInt16R.size());
	constexpr ___Format SignedFloat16Vec1(___FormatTypes::SignedFloat16Vec1, SignedFloat16R.size());
	constexpr ___Format UnsignedNormalized16Vec2(___FormatTypes::UnsignedNormalized16Vec2, UnsignedNormalized16RG.size());
	constexpr ___Format SignedNormalized16Vec2(___FormatTypes::SignedNormalized16Vec2, SignedNormalized16RG.size());
	constexpr ___Format UnsignedScaled16Vec2(___FormatTypes::UnsignedScaled16Vec2, UnsignedScaled16RG.size());
	constexpr ___Format SignedScaled16Vec2(___FormatTypes::SignedScaled16Vec2, SignedScaled16RG.size());
	constexpr ___Format UnsignedInt16Vec2(___FormatTypes::UnsignedInt16Vec2, UnsignedInt16RG.size());
	constexpr ___Format SignedInt16Vec2(___FormatTypes::SignedInt16Vec2, SignedInt16RG.size());
	constexpr ___Format SignedFloat16Vec2(___FormatTypes::SignedFloat16Vec2, SignedFloat16RG.size());
	constexpr ___Format UnsignedNormalized16Vec3(___FormatTypes::UnsignedNormalized16Vec3, UnsignedNormalized16RGB.size());
	constexpr ___Format SignedNormalized16Vec3(___FormatTypes::SignedNormalized16Vec3, SignedNormalized16RGB.size());
	constexpr ___Format UnsignedScaled16Vec3(___FormatTypes::UnsignedScaled16Vec3, UnsignedScaled16RGB.size());
	constexpr ___Format SignedScaled16Vec3(___FormatTypes::SignedScaled16Vec3, SignedScaled16RGB.size());
	constexpr ___Format UnsignedInt16Vec3(___FormatTypes::UnsignedInt16Vec3, UnsignedInt16RGB.size());
	constexpr ___Format SignedInt16Vec3(___FormatTypes::SignedInt16Vec3, SignedInt16RGB.size());
	constexpr ___Format SignedFloat16Vec3(___FormatTypes::SignedFloat16Vec3, SignedFloat16RGB.size());
	constexpr ___Format UnsignedNormalized16Vec4(___FormatTypes::UnsignedNormalized16Vec4, UnsignedNormalized16RGBA.size());
	constexpr ___Format SignedNormalized16Vec4(___FormatTypes::SignedNormalized16Vec4, SignedNormalized16RGBA.size());
	constexpr ___Format UnsignedScaled16Vec4(___FormatTypes::UnsignedScaled16Vec4, UnsignedScaled16RGBA.size());
	constexpr ___Format SignedScaled16Vec4(___FormatTypes::SignedScaled16Vec4, SignedScaled16RGBA.size());
	constexpr ___Format UnsignedInt16Vec4(___FormatTypes::UnsignedInt16Vec4, UnsignedInt16RGBA.size());
	constexpr ___Format SignedInt16Vec4(___FormatTypes::SignedInt16Vec4, SignedInt16RGBA.size());
	constexpr ___Format SignedFloat16Vec4(___FormatTypes::SignedFloat16Vec4, SignedFloat16RGBA.size());
	constexpr ___Format UnsignedInt32Vec1(___FormatTypes::UnsignedInt32Vec1, UnsignedInt32R.size());
	constexpr ___Format SignedInt32Vec1(___FormatTypes::SignedInt32Vec1, SignedInt32R.size());
	constexpr ___Format SignedFloat32Vec1(___FormatTypes::SignedFloat32Vec1, SignedFloat32R.size());
	constexpr ___Format UnsignedInt32Vec2(___FormatTypes::UnsignedInt32Vec2, UnsignedInt32RG.size());
	constexpr ___Format SignedInt32Vec2(___FormatTypes::SignedInt32Vec2, SignedInt32RG.size());
	constexpr ___Format SignedFloat32Vec2(___FormatTypes::SignedFloat32Vec2, SignedFloat32RG.size());
	constexpr ___Format UnsignedInt32Vec3(___FormatTypes::UnsignedInt32Vec3, UnsignedInt32RGB.size());
	constexpr ___Format SignedInt32Vec3(___FormatTypes::SignedInt32Vec3, SignedInt32RGB.size());
	constexpr ___Format SignedFloat32Vec3(___FormatTypes::SignedFloat32Vec3, SignedFloat32RGB.size());
	constexpr ___Format UnsignedInt32Vec4(___FormatTypes::UnsignedInt32Vec4, UnsignedInt32RGBA.size());
	constexpr ___Format SignedInt32Vec4(___FormatTypes::SignedInt32Vec4, SignedInt32RGBA.size());
	constexpr ___Format SignedFloat32Vec4(___FormatTypes::SignedFloat32Vec4, SignedFloat32RGBA.size());
	constexpr ___Format UnsignedInt64Vec1(___FormatTypes::UnsignedInt64Vec1, UnsignedInt64R.size());
	constexpr ___Format SignedInt64Vec1(___FormatTypes::SignedInt64Vec1, SignedInt64R.size());
	constexpr ___Format SignedFloat64Vec1(___FormatTypes::SignedFloat64Vec1, SignedFloat64R.size());
	constexpr ___Format UnsignedInt64Vec2(___FormatTypes::UnsignedInt64Vec2, UnsignedInt64RG.size());
	constexpr ___Format SignedInt64Vec2(___FormatTypes::SignedInt64Vec2, SignedInt64RG.size());
	constexpr ___Format SignedFloat64Vec2(___FormatTypes::SignedFloat64Vec2, SignedFloat64RG.size());
	constexpr ___Format UnsignedInt64Vec3(___FormatTypes::UnsignedInt64Vec3, UnsignedInt64RGB.size());
	constexpr ___Format SignedInt64Vec3(___FormatTypes::SignedInt64Vec3, SignedInt64RGB.size());
	constexpr ___Format SignedFloat64Vec3(___FormatTypes::SignedFloat64Vec3, SignedFloat64RGB.size());
	constexpr ___Format UnsignedInt64Vec4(___FormatTypes::UnsignedInt64Vec4, UnsignedInt64RGBA.size());
	constexpr ___Format SignedInt64Vec4(___FormatTypes::SignedInt64Vec4, SignedInt64RGBA.size());
	constexpr ___Format SignedFloat64Vec4(___FormatTypes::SignedFloat64Vec4, SignedFloat64RGBA.size());
	constexpr ___Format Normalized8R(___FormatTypes::Normalized8R, SignedNormalized8R.size());
	constexpr ___Format Scaled8R(___FormatTypes::Scaled8R, SignedScaled8R.size());
	constexpr ___Format Int8R(___FormatTypes::Int8R, SignedInt8R.size());
	constexpr ___Format Normalized8RG(___FormatTypes::Normalized8RG, SignedNormalized8RG.size());
	constexpr ___Format Scaled8RG(___FormatTypes::Scaled8RG, SignedScaled8RG.size());
	constexpr ___Format Int8RG(___FormatTypes::Int8RG, SignedInt8RG.size());
	constexpr ___Format Normalized8RGB(___FormatTypes::Normalized8RGB, SignedNormalized8RGB.size());
	constexpr ___Format Scaled8RGB(___FormatTypes::Scaled8RGB, SignedScaled8RGB.size());
	constexpr ___Format Int8RGB(___FormatTypes::Int8RGB, SignedInt8RGB.size());
	constexpr ___Format Normalized8BGR(___FormatTypes::Normalized8BGR, SignedNormalized8BGR.size());
	constexpr ___Format Scaled8BGR(___FormatTypes::Scaled8BGR, SignedScaled8BGR.size());
	constexpr ___Format Int8BGR(___FormatTypes::Int8BGR, SignedInt8BGR.size());
	constexpr ___Format Normalized8RGBA(___FormatTypes::Normalized8RGBA, SignedNormalized8RGBA.size());
	constexpr ___Format Scaled8RGBA(___FormatTypes::Scaled8RGBA, SignedScaled8RGBA.size());
	constexpr ___Format Int8RGBA(___FormatTypes::Int8RGBA, SignedInt8RGBA.size());
	constexpr ___Format Normalized8BGRA(___FormatTypes::Normalized8BGRA, SignedNormalized8BGRA.size());
	constexpr ___Format Scaled8BGRA(___FormatTypes::Scaled8BGRA, SignedScaled8BGRA.size());
	constexpr ___Format Int8BGRA(___FormatTypes::Int8BGRA, SignedInt8BGRA.size());
	constexpr ___Format Normalized8ABGRPACK32(___FormatTypes::Normalized8ABGRPACK32, SignedNormalized8ABGRPACK32.size());
	constexpr ___Format Scaled8ABGRPACK32(___FormatTypes::Scaled8ABGRPACK32, SignedScaled8ABGRPACK32.size());
	constexpr ___Format Int8ABGRPACK32(___FormatTypes::Int8ABGRPACK32, SignedInt8ABGRPACK32.size());
	constexpr ___Format Normalized16R(___FormatTypes::Normalized16R, SignedNormalized16R.size());
	constexpr ___Format Scaled16R(___FormatTypes::Scaled16R, SignedScaled16R.size());
	constexpr ___Format Int16R(___FormatTypes::Int16R, SignedInt16R.size());
	constexpr ___Format Float16R(___FormatTypes::Float16R, SignedFloat16R.size());
	constexpr ___Format Normalized16RG(___FormatTypes::Normalized16RG, SignedNormalized16RG.size());
	constexpr ___Format Scaled16RG(___FormatTypes::Scaled16RG, SignedScaled16RG.size());
	constexpr ___Format Int16RG(___FormatTypes::Int16RG, SignedInt16RG.size());
	constexpr ___Format Float16RG(___FormatTypes::Float16RG, SignedFloat16RG.size());
	constexpr ___Format Normalized16RGB(___FormatTypes::Normalized16RGB, SignedNormalized16RGB.size());
	constexpr ___Format Scaled16RGB(___FormatTypes::Scaled16RGB, SignedScaled16RGB.size());
	constexpr ___Format Int16RGB(___FormatTypes::Int16RGB, SignedInt16RGB.size());
	constexpr ___Format Float16RGB(___FormatTypes::Float16RGB, SignedFloat16RGB.size());
	constexpr ___Format Normalized16RGBA(___FormatTypes::Normalized16RGBA, SignedNormalized16RGBA.size());
	constexpr ___Format Scaled16RGBA(___FormatTypes::Scaled16RGBA, SignedScaled16RGBA.size());
	constexpr ___Format Int16RGBA(___FormatTypes::Int16RGBA, SignedInt16RGBA.size());
	constexpr ___Format Float16RGBA(___FormatTypes::Float16RGBA, SignedFloat16RGBA.size());
	constexpr ___Format Int32R(___FormatTypes::Int32R, SignedInt32R.size());
	constexpr ___Format Float32R(___FormatTypes::Float32R, SignedFloat32R.size());
	constexpr ___Format Int32RG(___FormatTypes::Int32RG, SignedInt32RG.size());
	constexpr ___Format Float32RG(___FormatTypes::Float32RG, SignedFloat32RG.size());
	constexpr ___Format Int32RGB(___FormatTypes::Int32RGB, SignedInt32RGB.size());
	constexpr ___Format Float32RGB(___FormatTypes::Float32RGB, SignedFloat32RGB.size());
	constexpr ___Format Int32RGBA(___FormatTypes::Int32RGBA, SignedInt32RGBA.size());
	constexpr ___Format Float32RGBA(___FormatTypes::Float32RGBA, SignedFloat32RGBA.size());
	constexpr ___Format Int64R(___FormatTypes::Int64R, SignedInt64R.size());
	constexpr ___Format Float64R(___FormatTypes::Float64R, SignedFloat64R.size());
	constexpr ___Format Int64RG(___FormatTypes::Int64RG, SignedInt64RG.size());
	constexpr ___Format Float64RG(___FormatTypes::Float64RG, SignedFloat64RG.size());
	constexpr ___Format Int64RGB(___FormatTypes::Int64RGB, SignedInt64RGB.size());
	constexpr ___Format Float64RGB(___FormatTypes::Float64RGB, SignedFloat64RGB.size());
	constexpr ___Format Int64RGBA(___FormatTypes::Int64RGBA, SignedInt64RGBA.size());
	constexpr ___Format Float64RGBA(___FormatTypes::Float64RGBA, SignedFloat64RGBA.size());
	constexpr ___Format Normalized8Vec1(___FormatTypes::Normalized8Vec1, SignedNormalized8Vec1.size());
	constexpr ___Format Scaled8Vec1(___FormatTypes::Scaled8Vec1, SignedScaled8Vec1.size());
	constexpr ___Format Int8Vec1(___FormatTypes::Int8Vec1, SignedInt8Vec1.size());
	constexpr ___Format Normalized8Vec2(___FormatTypes::Normalized8Vec2, SignedNormalized8Vec2.size());
	constexpr ___Format Scaled8Vec2(___FormatTypes::Scaled8Vec2, SignedScaled8Vec2.size());
	constexpr ___Format Int8Vec2(___FormatTypes::Int8Vec2, SignedInt8Vec2.size());
	constexpr ___Format Normalized8Vec3(___FormatTypes::Normalized8Vec3, SignedNormalized8Vec3.size());
	constexpr ___Format Scaled8Vec3(___FormatTypes::Scaled8Vec3, SignedScaled8Vec3.size());
	constexpr ___Format Int8Vec3(___FormatTypes::Int8Vec3, SignedInt8Vec3.size());
	constexpr ___Format Normalized8Vec4(___FormatTypes::Normalized8Vec4, SignedNormalized8Vec4.size());
	constexpr ___Format Scaled8Vec4(___FormatTypes::Scaled8Vec4, SignedScaled8Vec4.size());
	constexpr ___Format Int8Vec4(___FormatTypes::Int8Vec4, SignedInt8Vec4.size());
	constexpr ___Format Normalized16Vec1(___FormatTypes::Normalized16Vec1, SignedNormalized16Vec1.size());
	constexpr ___Format Scaled16Vec1(___FormatTypes::Scaled16Vec1, SignedScaled16Vec1.size());
	constexpr ___Format Int16Vec1(___FormatTypes::Int16Vec1, SignedInt16Vec1.size());
	constexpr ___Format Float16Vec1(___FormatTypes::Float16Vec1, SignedFloat16Vec1.size());
	constexpr ___Format Normalized16Vec2(___FormatTypes::Normalized16Vec2, SignedNormalized16Vec2.size());
	constexpr ___Format Scaled16Vec2(___FormatTypes::Scaled16Vec2, SignedScaled16Vec2.size());
	constexpr ___Format Int16Vec2(___FormatTypes::Int16Vec2, SignedInt16Vec2.size());
	constexpr ___Format Float16Vec2(___FormatTypes::Float16Vec2, SignedFloat16Vec2.size());
	constexpr ___Format Normalized16Vec3(___FormatTypes::Normalized16Vec3, SignedNormalized16Vec3.size());
	constexpr ___Format Scaled16Vec3(___FormatTypes::Scaled16Vec3, SignedScaled16Vec3.size());
	constexpr ___Format Int16Vec3(___FormatTypes::Int16Vec3, SignedInt16Vec3.size());
	constexpr ___Format Float16Vec3(___FormatTypes::Float16Vec3, SignedFloat16Vec3.size());
	constexpr ___Format Normalized16Vec4(___FormatTypes::Normalized16Vec4, SignedNormalized16Vec4.size());
	constexpr ___Format Scaled16Vec4(___FormatTypes::Scaled16Vec4, SignedScaled16Vec4.size());
	constexpr ___Format Int16Vec4(___FormatTypes::Int16Vec4, SignedInt16Vec4.size());
	constexpr ___Format Float16Vec4(___FormatTypes::Float16Vec4, SignedFloat16Vec4.size());
	constexpr ___Format Int32Vec1(___FormatTypes::Int32Vec1, SignedInt32Vec1.size());
	constexpr ___Format Float32Vec1(___FormatTypes::Float32Vec1, SignedFloat32Vec1.size());
	constexpr ___Format Int32Vec2(___FormatTypes::Int32Vec2, SignedInt32Vec2.size());
	constexpr ___Format Float32Vec2(___FormatTypes::Float32Vec2, SignedFloat32Vec2.size());
	constexpr ___Format Int32Vec3(___FormatTypes::Int32Vec3, SignedInt32Vec3.size());
	constexpr ___Format Float32Vec3(___FormatTypes::Float32Vec3, SignedFloat32Vec3.size());
	constexpr ___Format Int32Vec4(___FormatTypes::Int32Vec4, SignedInt32Vec4.size());
	constexpr ___Format Float32Vec4(___FormatTypes::Float32Vec4, SignedFloat32Vec4.size());
	constexpr ___Format Int64Vec1(___FormatTypes::Int64Vec1, SignedInt64Vec1.size());
	constexpr ___Format Float64Vec1(___FormatTypes::Float64Vec1, SignedFloat64Vec1.size());
	constexpr ___Format Int64Vec2(___FormatTypes::Int64Vec2, SignedInt64Vec2.size());
	constexpr ___Format Float64Vec2(___FormatTypes::Float64Vec2, SignedFloat64Vec2.size());
	constexpr ___Format Int64Vec3(___FormatTypes::Int64Vec3, SignedInt64Vec3.size());
	constexpr ___Format Float64Vec3(___FormatTypes::Float64Vec3, SignedFloat64Vec3.size());
	constexpr ___Format Int64Vec4(___FormatTypes::Int64Vec4, SignedInt64Vec4.size());
	constexpr ___Format Float64Vec4(___FormatTypes::Float64Vec4, SignedFloat64Vec4.size());
	constexpr ___Format UnsignedNormalized8(___FormatTypes::UnsignedNormalized8, UnsignedNormalized8R.size());
	constexpr ___Format SignedNormalized8(___FormatTypes::SignedNormalized8, SignedNormalized8R.size());
	constexpr ___Format UnsignedScaled8(___FormatTypes::UnsignedScaled8, UnsignedScaled8R.size());
	constexpr ___Format SignedScaled8(___FormatTypes::SignedScaled8, SignedScaled8R.size());
	constexpr ___Format UnsignedInt8(___FormatTypes::UnsignedInt8, UnsignedInt8R.size());
	constexpr ___Format SignedInt8(___FormatTypes::SignedInt8, SignedInt8R.size());
	constexpr ___Format Srgb8(___FormatTypes::Srgb8, Srgb8R.size());
	constexpr ___Format UnsignedNormalized16(___FormatTypes::UnsignedNormalized16, UnsignedNormalized16R.size());
	constexpr ___Format SignedNormalized16(___FormatTypes::SignedNormalized16, SignedNormalized16R.size());
	constexpr ___Format UnsignedScaled16(___FormatTypes::UnsignedScaled16, UnsignedScaled16R.size());
	constexpr ___Format SignedScaled16(___FormatTypes::SignedScaled16, SignedScaled16R.size());
	constexpr ___Format UnsignedInt16(___FormatTypes::UnsignedInt16, UnsignedInt16R.size());
	constexpr ___Format SignedInt16(___FormatTypes::SignedInt16, SignedInt16R.size());
	constexpr ___Format SignedFloat16(___FormatTypes::SignedFloat16, SignedFloat16R.size());
	constexpr ___Format UnsignedInt32(___FormatTypes::UnsignedInt32, UnsignedInt32R.size());
	constexpr ___Format SignedInt32(___FormatTypes::SignedInt32, SignedInt32R.size());
	constexpr ___Format SignedFloat32(___FormatTypes::SignedFloat32, SignedFloat32R.size());
	constexpr ___Format UnsignedInt64(___FormatTypes::UnsignedInt64, UnsignedInt64R.size());
	constexpr ___Format SignedInt64(___FormatTypes::SignedInt64, SignedInt64R.size());
	constexpr ___Format SignedFloat64(___FormatTypes::SignedFloat64, SignedFloat64R.size());
	constexpr ___Format Normalized8(___FormatTypes::Normalized8, SignedNormalized8Vec1.size());
	constexpr ___Format Scaled8(___FormatTypes::Scaled8, SignedScaled8Vec1.size());
	constexpr ___Format Int8(___FormatTypes::Int8, SignedInt8Vec1.size());
	constexpr ___Format Normalized16(___FormatTypes::Normalized16, SignedNormalized16Vec1.size());
	constexpr ___Format Scaled16(___FormatTypes::Scaled16, SignedScaled16Vec1.size());
	constexpr ___Format Int16(___FormatTypes::Int16, SignedInt16Vec1.size());
	constexpr ___Format Float16(___FormatTypes::Float16, SignedFloat16Vec1.size());
	constexpr ___Format Int32(___FormatTypes::Int32, SignedInt32Vec1.size());
	constexpr ___Format Float32(___FormatTypes::Float32, SignedFloat32Vec1.size());
	constexpr ___Format Int64(___FormatTypes::Int64, SignedInt64Vec1.size());
	constexpr ___Format Float64(___FormatTypes::Float64, SignedFloat64Vec1.size());
	constexpr ___Format UnsignedIntR(___FormatTypes::UnsignedIntR, UnsignedInt32R.size());
	constexpr ___Format SignedIntR(___FormatTypes::SignedIntR, SignedInt32R.size());
	constexpr ___Format SignedFloatR(___FormatTypes::SignedFloatR, SignedFloat32R.size());
	constexpr ___Format UnsignedIntRG(___FormatTypes::UnsignedIntRG, UnsignedInt32RG.size());
	constexpr ___Format SignedIntRG(___FormatTypes::SignedIntRG, SignedInt32RG.size());
	constexpr ___Format SignedFloatRG(___FormatTypes::SignedFloatRG, SignedFloat32RG.size());
	constexpr ___Format UnsignedIntRGB(___FormatTypes::UnsignedIntRGB, UnsignedInt32RGB.size());
	constexpr ___Format SignedIntRGB(___FormatTypes::SignedIntRGB, SignedInt32RGB.size());
	constexpr ___Format SignedFloatRGB(___FormatTypes::SignedFloatRGB, SignedFloat32RGB.size());
	constexpr ___Format UnsignedIntRGBA(___FormatTypes::UnsignedIntRGBA, UnsignedInt32RGBA.size());
	constexpr ___Format SignedIntRGBA(___FormatTypes::SignedIntRGBA, SignedInt32RGBA.size());
	constexpr ___Format SignedFloatRGBA(___FormatTypes::SignedFloatRGBA, SignedFloat32RGBA.size());
	constexpr ___Format UnsignedIntVec1(___FormatTypes::UnsignedIntVec1, UnsignedInt32R.size());
	constexpr ___Format SignedIntVec1(___FormatTypes::SignedIntVec1, SignedInt32R.size());
	constexpr ___Format SignedFloatVec1(___FormatTypes::SignedFloatVec1, SignedFloat32R.size());
	constexpr ___Format UnsignedIntVec2(___FormatTypes::UnsignedIntVec2, UnsignedInt32RG.size());
	constexpr ___Format SignedIntVec2(___FormatTypes::SignedIntVec2, SignedInt32RG.size());
	constexpr ___Format SignedFloatVec2(___FormatTypes::SignedFloatVec2, SignedFloat32RG.size());
	constexpr ___Format UnsignedIntVec3(___FormatTypes::UnsignedIntVec3, UnsignedInt32RGB.size());
	constexpr ___Format SignedIntVec3(___FormatTypes::SignedIntVec3, SignedInt32RGB.size());
	constexpr ___Format SignedFloatVec3(___FormatTypes::SignedFloatVec3, SignedFloat32RGB.size());
	constexpr ___Format UnsignedIntVec4(___FormatTypes::UnsignedIntVec4, UnsignedInt32RGBA.size());
	constexpr ___Format SignedIntVec4(___FormatTypes::SignedIntVec4, SignedInt32RGBA.size());
	constexpr ___Format SignedFloatVec4(___FormatTypes::SignedFloatVec4, SignedFloat32RGBA.size());
	constexpr ___Format IntR(___FormatTypes::IntR, SignedInt32R.size());
	constexpr ___Format FloatR(___FormatTypes::FloatR, SignedFloat32R.size());
	constexpr ___Format IntRG(___FormatTypes::IntRG, SignedInt32RG.size());
	constexpr ___Format FloatRG(___FormatTypes::FloatRG, SignedFloat32RG.size());
	constexpr ___Format IntRGB(___FormatTypes::IntRGB, SignedInt32RGB.size());
	constexpr ___Format FloatRGB(___FormatTypes::FloatRGB, SignedFloat32RGB.size());
	constexpr ___Format IntRGBA(___FormatTypes::IntRGBA, SignedInt32RGBA.size());
	constexpr ___Format FloatRGBA(___FormatTypes::FloatRGBA, SignedFloat32RGBA.size());
	constexpr ___Format IntVec1(___FormatTypes::IntVec1, SignedInt32Vec1.size());
	constexpr ___Format FloatVec1(___FormatTypes::FloatVec1, SignedFloat32Vec1.size());
	constexpr ___Format IntVec2(___FormatTypes::IntVec2, SignedInt32Vec2.size());
	constexpr ___Format FloatVec2(___FormatTypes::FloatVec2, SignedFloat32Vec2.size());
	constexpr ___Format IntVec3(___FormatTypes::IntVec3, SignedInt32Vec3.size());
	constexpr ___Format FloatVec3(___FormatTypes::FloatVec3, SignedFloat32Vec3.size());
	constexpr ___Format IntVec4(___FormatTypes::IntVec4, SignedInt32Vec4.size());
	constexpr ___Format FloatVec4(___FormatTypes::FloatVec4, SignedFloat32Vec4.size());
	constexpr ___Format UnsignedInt(___FormatTypes::UnsignedInt, UnsignedInt32R.size());
	constexpr ___Format SignedInt(___FormatTypes::SignedInt, SignedInt32R.size());
	constexpr ___Format SignedFloat(___FormatTypes::SignedFloat, SignedFloat32R.size());
	constexpr ___Format Int(___FormatTypes::Int, SignedInt32Vec1.size());
	constexpr ___Format Float(___FormatTypes::Float, SignedFloat32Vec1.size());
}

namespace VSL_NAMESPACE::concepts {
	template<typename T>
	concept ___is_convertible_graphic_type_by_static_method = requires {
		{  T::graphic_type } -> std::convertible_to<data_format::___Format>;
	};

	template<typename T>
	concept ___is_convertible_graphic_type_by_dynamic_method = requires (const T & t) {
		{ t.get_graphic_type() } -> std::convertible_to<data_format::___Format>;
	};

	template<typename T>
	concept is_static_convertible_graphic_type = (___is_convertible_graphic_type_by_static_method<T>) ||
		(std::same_as<T, char>) ||
		(std::same_as<T, unsigned char>) ||
		(std::same_as<T, short>) ||
		(std::same_as<T, unsigned short>) ||
		(std::same_as<T, int>) ||
		(std::same_as<T, unsigned int>) ||
		(std::same_as<T, long>) ||
		(std::same_as<T, unsigned long>) ||
		(std::same_as<T, long long>) ||
		(std::same_as<T, unsigned long long>) ||
		(std::same_as<T, float>) ||
		(std::same_as<T, double>);

	template<typename T>
	concept is_convertible_graphic_type = is_static_convertible_graphic_type<T> || ___is_convertible_graphic_type_by_dynamic_method<T>;
}

namespace VSL_NAMESPACE::data_format {
	template<typename T>
	consteval ___Format convert_graphic_type() {
		if constexpr (not concepts::is_static_convertible_graphic_type<T>)
			return Undefined;
		if constexpr (concepts::___is_convertible_graphic_type_by_static_method<T>)
			return T::graphic_type;
		if constexpr (std::same_as<T, char> == true)
			return Int8;
		if constexpr (std::same_as<T, unsigned char> == true)
			return UnsignedInt8;
		if constexpr (std::same_as<T, short> == true)
			return Int16;
		if constexpr (std::same_as<T, unsigned short> == true)
			return UnsignedInt16;
		if constexpr (std::same_as<T, int> == true)
			return Int;
		if constexpr (std::same_as<T, unsigned int> == true)
			return UnsignedInt;
		if constexpr (sizeof(long) == 4) {
			if constexpr (std::same_as<T, long> == true)
				return Int;
			if constexpr (std::same_as<T, unsigned long> == true)
				return UnsignedInt;
		}
		else if constexpr (sizeof(long) == 8) {
			if constexpr (std::same_as<T, long> == true)
				return Int64;
			if constexpr (std::same_as<T, unsigned long> == true)
				return UnsignedInt64;
		}
		if constexpr (std::same_as<T, long long> == true)
			return Int64;
		if constexpr (std::same_as<T, unsigned long long> == true)
			return UnsignedInt64;
		if constexpr (std::same_as<T, float> == true)
			return Float;
		if constexpr (std::same_as<T, double> == true)
			return Float64;
		return Undefined;
	}

	template<typename T>
	consteval ___Format convert_graphic_type(T v) {
		if constexpr (not concepts::is_convertible_graphic_type<T>)
			return Undefined;
		if constexpr (concepts::is_static_convertible_graphic_type<T>)
			return convert_graphic_type<T>();
		return v.get_graphic_type();
	}

	template<size_t Length>
	inline constexpr ___Format ___Format::toVec() const
	{
		if constexpr (Length == 1) {
				if (___FormatTypes::UnsignedNormalized8R == _formatType)
					return UnsignedNormalized8R;
				if (___FormatTypes::SignedNormalized8R == _formatType)
					return SignedNormalized8R;
				if (___FormatTypes::UnsignedScaled8R == _formatType)
					return UnsignedScaled8R;
				if (___FormatTypes::SignedScaled8R == _formatType)
					return SignedScaled8R;
				if (___FormatTypes::UnsignedInt8R == _formatType)
					return UnsignedInt8R;
				if (___FormatTypes::SignedInt8R == _formatType)
					return SignedInt8R;
				if (___FormatTypes::Srgb8R == _formatType)
					return Srgb8R;
				if (___FormatTypes::UnsignedNormalized8RG == _formatType)
					return UnsignedNormalized8R;
				if (___FormatTypes::SignedNormalized8RG == _formatType)
					return SignedNormalized8R;
				if (___FormatTypes::UnsignedScaled8RG == _formatType)
					return UnsignedScaled8R;
				if (___FormatTypes::SignedScaled8RG == _formatType)
					return SignedScaled8R;
				if (___FormatTypes::UnsignedInt8RG == _formatType)
					return UnsignedInt8R;
				if (___FormatTypes::SignedInt8RG == _formatType)
					return SignedInt8R;
				if (___FormatTypes::Srgb8RG == _formatType)
					return Srgb8R;
				if (___FormatTypes::UnsignedNormalized8RGB == _formatType)
					return UnsignedNormalized8R;
				if (___FormatTypes::SignedNormalized8RGB == _formatType)
					return SignedNormalized8R;
				if (___FormatTypes::UnsignedScaled8RGB == _formatType)
					return UnsignedScaled8R;
				if (___FormatTypes::SignedScaled8RGB == _formatType)
					return SignedScaled8R;
				if (___FormatTypes::UnsignedInt8RGB == _formatType)
					return UnsignedInt8R;
				if (___FormatTypes::SignedInt8RGB == _formatType)
					return SignedInt8R;
				if (___FormatTypes::Srgb8RGB == _formatType)
					return Srgb8R;
				if (___FormatTypes::UnsignedNormalized8BGR == _formatType)
					return UnsignedNormalized8R;
				if (___FormatTypes::SignedNormalized8BGR == _formatType)
					return SignedNormalized8R;
				if (___FormatTypes::UnsignedScaled8BGR == _formatType)
					return UnsignedScaled8R;
				if (___FormatTypes::SignedScaled8BGR == _formatType)
					return SignedScaled8R;
				if (___FormatTypes::UnsignedInt8BGR == _formatType)
					return UnsignedInt8R;
				if (___FormatTypes::SignedInt8BGR == _formatType)
					return SignedInt8R;
				if (___FormatTypes::Srgb8BGR == _formatType)
					return Srgb8R;
				if (___FormatTypes::UnsignedNormalized8RGBA == _formatType)
					return UnsignedNormalized8R;
				if (___FormatTypes::SignedNormalized8RGBA == _formatType)
					return SignedNormalized8R;
				if (___FormatTypes::UnsignedScaled8RGBA == _formatType)
					return UnsignedScaled8R;
				if (___FormatTypes::SignedScaled8RGBA == _formatType)
					return SignedScaled8R;
				if (___FormatTypes::UnsignedInt8RGBA == _formatType)
					return UnsignedInt8R;
				if (___FormatTypes::SignedInt8RGBA == _formatType)
					return SignedInt8R;
				if (___FormatTypes::Srgb8RGBA == _formatType)
					return Srgb8R;
				if (___FormatTypes::UnsignedNormalized8BGRA == _formatType)
					return UnsignedNormalized8R;
				if (___FormatTypes::SignedNormalized8BGRA == _formatType)
					return SignedNormalized8R;
				if (___FormatTypes::UnsignedScaled8BGRA == _formatType)
					return UnsignedScaled8R;
				if (___FormatTypes::SignedScaled8BGRA == _formatType)
					return SignedScaled8R;
				if (___FormatTypes::UnsignedInt8BGRA == _formatType)
					return UnsignedInt8R;
				if (___FormatTypes::SignedInt8BGRA == _formatType)
					return SignedInt8R;
				if (___FormatTypes::Srgb8BGRA == _formatType)
					return Srgb8R;
				if (___FormatTypes::UnsignedNormalized8ABGRPACK32 == _formatType)
					return UnsignedNormalized8R;
				if (___FormatTypes::SignedNormalized8ABGRPACK32 == _formatType)
					return SignedNormalized8R;
				if (___FormatTypes::UnsignedScaled8ABGRPACK32 == _formatType)
					return UnsignedScaled8R;
				if (___FormatTypes::SignedScaled8ABGRPACK32 == _formatType)
					return SignedScaled8R;
				if (___FormatTypes::UnsignedInt8ABGRPACK32 == _formatType)
					return UnsignedInt8R;
				if (___FormatTypes::SignedInt8ABGRPACK32 == _formatType)
					return SignedInt8R;
				if (___FormatTypes::Srgb8ABGRPACK32 == _formatType)
					return Srgb8R;
				if (___FormatTypes::UnsignedNormalized16R == _formatType)
					return UnsignedNormalized16R;
				if (___FormatTypes::SignedNormalized16R == _formatType)
					return SignedNormalized16R;
				if (___FormatTypes::UnsignedScaled16R == _formatType)
					return UnsignedScaled16R;
				if (___FormatTypes::SignedScaled16R == _formatType)
					return SignedScaled16R;
				if (___FormatTypes::UnsignedInt16R == _formatType)
					return UnsignedInt16R;
				if (___FormatTypes::SignedInt16R == _formatType)
					return SignedInt16R;
				if (___FormatTypes::SignedFloat16R == _formatType)
					return SignedFloat16R;
				if (___FormatTypes::UnsignedNormalized16RG == _formatType)
					return UnsignedNormalized16R;
				if (___FormatTypes::SignedNormalized16RG == _formatType)
					return SignedNormalized16R;
				if (___FormatTypes::UnsignedScaled16RG == _formatType)
					return UnsignedScaled16R;
				if (___FormatTypes::SignedScaled16RG == _formatType)
					return SignedScaled16R;
				if (___FormatTypes::UnsignedInt16RG == _formatType)
					return UnsignedInt16R;
				if (___FormatTypes::SignedInt16RG == _formatType)
					return SignedInt16R;
				if (___FormatTypes::SignedFloat16RG == _formatType)
					return SignedFloat16R;
				if (___FormatTypes::UnsignedNormalized16RGB == _formatType)
					return UnsignedNormalized16R;
				if (___FormatTypes::SignedNormalized16RGB == _formatType)
					return SignedNormalized16R;
				if (___FormatTypes::UnsignedScaled16RGB == _formatType)
					return UnsignedScaled16R;
				if (___FormatTypes::SignedScaled16RGB == _formatType)
					return SignedScaled16R;
				if (___FormatTypes::UnsignedInt16RGB == _formatType)
					return UnsignedInt16R;
				if (___FormatTypes::SignedInt16RGB == _formatType)
					return SignedInt16R;
				if (___FormatTypes::SignedFloat16RGB == _formatType)
					return SignedFloat16R;
				if (___FormatTypes::UnsignedNormalized16RGBA == _formatType)
					return UnsignedNormalized16R;
				if (___FormatTypes::SignedNormalized16RGBA == _formatType)
					return SignedNormalized16R;
				if (___FormatTypes::UnsignedScaled16RGBA == _formatType)
					return UnsignedScaled16R;
				if (___FormatTypes::SignedScaled16RGBA == _formatType)
					return SignedScaled16R;
				if (___FormatTypes::UnsignedInt16RGBA == _formatType)
					return UnsignedInt16R;
				if (___FormatTypes::SignedInt16RGBA == _formatType)
					return SignedInt16R;
				if (___FormatTypes::SignedFloat16RGBA == _formatType)
					return SignedFloat16R;
				if (___FormatTypes::UnsignedInt32R == _formatType)
					return UnsignedInt32R;
				if (___FormatTypes::SignedInt32R == _formatType)
					return SignedInt32R;
				if (___FormatTypes::SignedFloat32R == _formatType)
					return SignedFloat32R;
				if (___FormatTypes::UnsignedInt32RG == _formatType)
					return UnsignedInt32R;
				if (___FormatTypes::SignedInt32RG == _formatType)
					return SignedInt32R;
				if (___FormatTypes::SignedFloat32RG == _formatType)
					return SignedFloat32R;
				if (___FormatTypes::UnsignedInt32RGB == _formatType)
					return UnsignedInt32R;
				if (___FormatTypes::SignedInt32RGB == _formatType)
					return SignedInt32R;
				if (___FormatTypes::SignedFloat32RGB == _formatType)
					return SignedFloat32R;
				if (___FormatTypes::UnsignedInt32RGBA == _formatType)
					return UnsignedInt32R;
				if (___FormatTypes::SignedInt32RGBA == _formatType)
					return SignedInt32R;
				if (___FormatTypes::SignedFloat32RGBA == _formatType)
					return SignedFloat32R;
				if (___FormatTypes::UnsignedInt64R == _formatType)
					return UnsignedInt64R;
				if (___FormatTypes::SignedInt64R == _formatType)
					return SignedInt64R;
				if (___FormatTypes::SignedFloat64R == _formatType)
					return SignedFloat64R;
				if (___FormatTypes::UnsignedInt64RG == _formatType)
					return UnsignedInt64R;
				if (___FormatTypes::SignedInt64RG == _formatType)
					return SignedInt64R;
				if (___FormatTypes::SignedFloat64RG == _formatType)
					return SignedFloat64R;
				if (___FormatTypes::UnsignedInt64RGB == _formatType)
					return UnsignedInt64R;
				if (___FormatTypes::SignedInt64RGB == _formatType)
					return SignedInt64R;
				if (___FormatTypes::SignedFloat64RGB == _formatType)
					return SignedFloat64R;
				if (___FormatTypes::UnsignedInt64RGBA == _formatType)
					return UnsignedInt64R;
				if (___FormatTypes::SignedInt64RGBA == _formatType)
					return SignedInt64R;
				if (___FormatTypes::SignedFloat64RGBA == _formatType)
					return SignedFloat64R;
		}
		if constexpr (Length == 2) {
			if (___FormatTypes::UnsignedNormalized8R == _formatType)
				return UnsignedNormalized8RG;
			if (___FormatTypes::SignedNormalized8R == _formatType)
				return SignedNormalized8RG;
			if (___FormatTypes::UnsignedScaled8R == _formatType)
				return UnsignedScaled8RG;
			if (___FormatTypes::SignedScaled8R == _formatType)
				return SignedScaled8RG;
			if (___FormatTypes::UnsignedInt8R == _formatType)
				return UnsignedInt8RG;
			if (___FormatTypes::SignedInt8R == _formatType)
				return SignedInt8RG;
			if (___FormatTypes::Srgb8R == _formatType)
				return Srgb8RG;
			if (___FormatTypes::UnsignedNormalized8RG == _formatType)
				return UnsignedNormalized8RG;
			if (___FormatTypes::SignedNormalized8RG == _formatType)
				return SignedNormalized8RG;
			if (___FormatTypes::UnsignedScaled8RG == _formatType)
				return UnsignedScaled8RG;
			if (___FormatTypes::SignedScaled8RG == _formatType)
				return SignedScaled8RG;
			if (___FormatTypes::UnsignedInt8RG == _formatType)
				return UnsignedInt8RG;
			if (___FormatTypes::SignedInt8RG == _formatType)
				return SignedInt8RG;
			if (___FormatTypes::Srgb8RG == _formatType)
				return Srgb8RG;
			if (___FormatTypes::UnsignedNormalized8RGB == _formatType)
				return UnsignedNormalized8RG;
			if (___FormatTypes::SignedNormalized8RGB == _formatType)
				return SignedNormalized8RG;
			if (___FormatTypes::UnsignedScaled8RGB == _formatType)
				return UnsignedScaled8RG;
			if (___FormatTypes::SignedScaled8RGB == _formatType)
				return SignedScaled8RG;
			if (___FormatTypes::UnsignedInt8RGB == _formatType)
				return UnsignedInt8RG;
			if (___FormatTypes::SignedInt8RGB == _formatType)
				return SignedInt8RG;
			if (___FormatTypes::Srgb8RGB == _formatType)
				return Srgb8RG;
			if (___FormatTypes::UnsignedNormalized8BGR == _formatType)
				return UnsignedNormalized8RG;
			if (___FormatTypes::SignedNormalized8BGR == _formatType)
				return SignedNormalized8RG;
			if (___FormatTypes::UnsignedScaled8BGR == _formatType)
				return UnsignedScaled8RG;
			if (___FormatTypes::SignedScaled8BGR == _formatType)
				return SignedScaled8RG;
			if (___FormatTypes::UnsignedInt8BGR == _formatType)
				return UnsignedInt8RG;
			if (___FormatTypes::SignedInt8BGR == _formatType)
				return SignedInt8RG;
			if (___FormatTypes::Srgb8BGR == _formatType)
				return Srgb8RG;
			if (___FormatTypes::UnsignedNormalized8RGBA == _formatType)
				return UnsignedNormalized8RG;
			if (___FormatTypes::SignedNormalized8RGBA == _formatType)
				return SignedNormalized8RG;
			if (___FormatTypes::UnsignedScaled8RGBA == _formatType)
				return UnsignedScaled8RG;
			if (___FormatTypes::SignedScaled8RGBA == _formatType)
				return SignedScaled8RG;
			if (___FormatTypes::UnsignedInt8RGBA == _formatType)
				return UnsignedInt8RG;
			if (___FormatTypes::SignedInt8RGBA == _formatType)
				return SignedInt8RG;
			if (___FormatTypes::Srgb8RGBA == _formatType)
				return Srgb8RG;
			if (___FormatTypes::UnsignedNormalized8BGRA == _formatType)
				return UnsignedNormalized8RG;
			if (___FormatTypes::SignedNormalized8BGRA == _formatType)
				return SignedNormalized8RG;
			if (___FormatTypes::UnsignedScaled8BGRA == _formatType)
				return UnsignedScaled8RG;
			if (___FormatTypes::SignedScaled8BGRA == _formatType)
				return SignedScaled8RG;
			if (___FormatTypes::UnsignedInt8BGRA == _formatType)
				return UnsignedInt8RG;
			if (___FormatTypes::SignedInt8BGRA == _formatType)
				return SignedInt8RG;
			if (___FormatTypes::Srgb8BGRA == _formatType)
				return Srgb8RG;
			if (___FormatTypes::UnsignedNormalized8ABGRPACK32 == _formatType)
				return UnsignedNormalized8RG;
			if (___FormatTypes::SignedNormalized8ABGRPACK32 == _formatType)
				return SignedNormalized8RG;
			if (___FormatTypes::UnsignedScaled8ABGRPACK32 == _formatType)
				return UnsignedScaled8RG;
			if (___FormatTypes::SignedScaled8ABGRPACK32 == _formatType)
				return SignedScaled8RG;
			if (___FormatTypes::UnsignedInt8ABGRPACK32 == _formatType)
				return UnsignedInt8RG;
			if (___FormatTypes::SignedInt8ABGRPACK32 == _formatType)
				return SignedInt8RG;
			if (___FormatTypes::Srgb8ABGRPACK32 == _formatType)
				return Srgb8RG;
			if (___FormatTypes::UnsignedNormalized16R == _formatType)
				return UnsignedNormalized16RG;
			if (___FormatTypes::SignedNormalized16R == _formatType)
				return SignedNormalized16RG;
			if (___FormatTypes::UnsignedScaled16R == _formatType)
				return UnsignedScaled16RG;
			if (___FormatTypes::SignedScaled16R == _formatType)
				return SignedScaled16RG;
			if (___FormatTypes::UnsignedInt16R == _formatType)
				return UnsignedInt16RG;
			if (___FormatTypes::SignedInt16R == _formatType)
				return SignedInt16RG;
			if (___FormatTypes::SignedFloat16R == _formatType)
				return SignedFloat16RG;
			if (___FormatTypes::UnsignedNormalized16RG == _formatType)
				return UnsignedNormalized16RG;
			if (___FormatTypes::SignedNormalized16RG == _formatType)
				return SignedNormalized16RG;
			if (___FormatTypes::UnsignedScaled16RG == _formatType)
				return UnsignedScaled16RG;
			if (___FormatTypes::SignedScaled16RG == _formatType)
				return SignedScaled16RG;
			if (___FormatTypes::UnsignedInt16RG == _formatType)
				return UnsignedInt16RG;
			if (___FormatTypes::SignedInt16RG == _formatType)
				return SignedInt16RG;
			if (___FormatTypes::SignedFloat16RG == _formatType)
				return SignedFloat16RG;
			if (___FormatTypes::UnsignedNormalized16RGB == _formatType)
				return UnsignedNormalized16RG;
			if (___FormatTypes::SignedNormalized16RGB == _formatType)
				return SignedNormalized16RG;
			if (___FormatTypes::UnsignedScaled16RGB == _formatType)
				return UnsignedScaled16RG;
			if (___FormatTypes::SignedScaled16RGB == _formatType)
				return SignedScaled16RG;
			if (___FormatTypes::UnsignedInt16RGB == _formatType)
				return UnsignedInt16RG;
			if (___FormatTypes::SignedInt16RGB == _formatType)
				return SignedInt16RG;
			if (___FormatTypes::SignedFloat16RGB == _formatType)
				return SignedFloat16RG;
			if (___FormatTypes::UnsignedNormalized16RGBA == _formatType)
				return UnsignedNormalized16RG;
			if (___FormatTypes::SignedNormalized16RGBA == _formatType)
				return SignedNormalized16RG;
			if (___FormatTypes::UnsignedScaled16RGBA == _formatType)
				return UnsignedScaled16RG;
			if (___FormatTypes::SignedScaled16RGBA == _formatType)
				return SignedScaled16RG;
			if (___FormatTypes::UnsignedInt16RGBA == _formatType)
				return UnsignedInt16RG;
			if (___FormatTypes::SignedInt16RGBA == _formatType)
				return SignedInt16RG;
			if (___FormatTypes::SignedFloat16RGBA == _formatType)
				return SignedFloat16RG;
			if (___FormatTypes::UnsignedInt32R == _formatType)
				return UnsignedInt32RG;
			if (___FormatTypes::SignedInt32R == _formatType)
				return SignedInt32RG;
			if (___FormatTypes::SignedFloat32R == _formatType)
				return SignedFloat32RG;
			if (___FormatTypes::UnsignedInt32RG == _formatType)
				return UnsignedInt32RG;
			if (___FormatTypes::SignedInt32RG == _formatType)
				return SignedInt32RG;
			if (___FormatTypes::SignedFloat32RG == _formatType)
				return SignedFloat32RG;
			if (___FormatTypes::UnsignedInt32RGB == _formatType)
				return UnsignedInt32RG;
			if (___FormatTypes::SignedInt32RGB == _formatType)
				return SignedInt32RG;
			if (___FormatTypes::SignedFloat32RGB == _formatType)
				return SignedFloat32RG;
			if (___FormatTypes::UnsignedInt32RGBA == _formatType)
				return UnsignedInt32RG;
			if (___FormatTypes::SignedInt32RGBA == _formatType)
				return SignedInt32RG;
			if (___FormatTypes::SignedFloat32RGBA == _formatType)
				return SignedFloat32RG;
			if (___FormatTypes::UnsignedInt64R == _formatType)
				return UnsignedInt64RG;
			if (___FormatTypes::SignedInt64R == _formatType)
				return SignedInt64RG;
			if (___FormatTypes::SignedFloat64R == _formatType)
				return SignedFloat64RG;
			if (___FormatTypes::UnsignedInt64RG == _formatType)
				return UnsignedInt64RG;
			if (___FormatTypes::SignedInt64RG == _formatType)
				return SignedInt64RG;
			if (___FormatTypes::SignedFloat64RG == _formatType)
				return SignedFloat64RG;
			if (___FormatTypes::UnsignedInt64RGB == _formatType)
				return UnsignedInt64RG;
			if (___FormatTypes::SignedInt64RGB == _formatType)
				return SignedInt64RG;
			if (___FormatTypes::SignedFloat64RGB == _formatType)
				return SignedFloat64RG;
			if (___FormatTypes::UnsignedInt64RGBA == _formatType)
				return UnsignedInt64RG;
			if (___FormatTypes::SignedInt64RGBA == _formatType)
				return SignedInt64RG;
			if (___FormatTypes::SignedFloat64RGBA == _formatType)
				return SignedFloat64RG;
		}
		if constexpr (Length == 3) {
			if (___FormatTypes::UnsignedNormalized8R == _formatType)
				return UnsignedNormalized8RGB;
			if (___FormatTypes::SignedNormalized8R == _formatType)
				return SignedNormalized8RGB;
			if (___FormatTypes::UnsignedScaled8R == _formatType)
				return UnsignedScaled8RGB;
			if (___FormatTypes::SignedScaled8R == _formatType)
				return SignedScaled8RGB;
			if (___FormatTypes::UnsignedInt8R == _formatType)
				return UnsignedInt8RGB;
			if (___FormatTypes::SignedInt8R == _formatType)
				return SignedInt8RGB;
			if (___FormatTypes::Srgb8R == _formatType)
				return Srgb8RGB;
			if (___FormatTypes::UnsignedNormalized8RG == _formatType)
				return UnsignedNormalized8RGB;
			if (___FormatTypes::SignedNormalized8RG == _formatType)
				return SignedNormalized8RGB;
			if (___FormatTypes::UnsignedScaled8RG == _formatType)
				return UnsignedScaled8RGB;
			if (___FormatTypes::SignedScaled8RG == _formatType)
				return SignedScaled8RGB;
			if (___FormatTypes::UnsignedInt8RG == _formatType)
				return UnsignedInt8RGB;
			if (___FormatTypes::SignedInt8RG == _formatType)
				return SignedInt8RGB;
			if (___FormatTypes::Srgb8RG == _formatType)
				return Srgb8RGB;
			if (___FormatTypes::UnsignedNormalized8RGB == _formatType)
				return UnsignedNormalized8RGB;
			if (___FormatTypes::SignedNormalized8RGB == _formatType)
				return SignedNormalized8RGB;
			if (___FormatTypes::UnsignedScaled8RGB == _formatType)
				return UnsignedScaled8RGB;
			if (___FormatTypes::SignedScaled8RGB == _formatType)
				return SignedScaled8RGB;
			if (___FormatTypes::UnsignedInt8RGB == _formatType)
				return UnsignedInt8RGB;
			if (___FormatTypes::SignedInt8RGB == _formatType)
				return SignedInt8RGB;
			if (___FormatTypes::Srgb8RGB == _formatType)
				return Srgb8RGB;
			if (___FormatTypes::UnsignedNormalized8BGR == _formatType)
				return UnsignedNormalized8RGB;
			if (___FormatTypes::SignedNormalized8BGR == _formatType)
				return SignedNormalized8RGB;
			if (___FormatTypes::UnsignedScaled8BGR == _formatType)
				return UnsignedScaled8RGB;
			if (___FormatTypes::SignedScaled8BGR == _formatType)
				return SignedScaled8RGB;
			if (___FormatTypes::UnsignedInt8BGR == _formatType)
				return UnsignedInt8RGB;
			if (___FormatTypes::SignedInt8BGR == _formatType)
				return SignedInt8RGB;
			if (___FormatTypes::Srgb8BGR == _formatType)
				return Srgb8RGB;
			if (___FormatTypes::UnsignedNormalized8RGBA == _formatType)
				return UnsignedNormalized8RGB;
			if (___FormatTypes::SignedNormalized8RGBA == _formatType)
				return SignedNormalized8RGB;
			if (___FormatTypes::UnsignedScaled8RGBA == _formatType)
				return UnsignedScaled8RGB;
			if (___FormatTypes::SignedScaled8RGBA == _formatType)
				return SignedScaled8RGB;
			if (___FormatTypes::UnsignedInt8RGBA == _formatType)
				return UnsignedInt8RGB;
			if (___FormatTypes::SignedInt8RGBA == _formatType)
				return SignedInt8RGB;
			if (___FormatTypes::Srgb8RGBA == _formatType)
				return Srgb8RGB;
			if (___FormatTypes::UnsignedNormalized8BGRA == _formatType)
				return UnsignedNormalized8RGB;
			if (___FormatTypes::SignedNormalized8BGRA == _formatType)
				return SignedNormalized8RGB;
			if (___FormatTypes::UnsignedScaled8BGRA == _formatType)
				return UnsignedScaled8RGB;
			if (___FormatTypes::SignedScaled8BGRA == _formatType)
				return SignedScaled8RGB;
			if (___FormatTypes::UnsignedInt8BGRA == _formatType)
				return UnsignedInt8RGB;
			if (___FormatTypes::SignedInt8BGRA == _formatType)
				return SignedInt8RGB;
			if (___FormatTypes::Srgb8BGRA == _formatType)
				return Srgb8RGB;
			if (___FormatTypes::UnsignedNormalized8ABGRPACK32 == _formatType)
				return UnsignedNormalized8RGB;
			if (___FormatTypes::SignedNormalized8ABGRPACK32 == _formatType)
				return SignedNormalized8RGB;
			if (___FormatTypes::UnsignedScaled8ABGRPACK32 == _formatType)
				return UnsignedScaled8RGB;
			if (___FormatTypes::SignedScaled8ABGRPACK32 == _formatType)
				return SignedScaled8RGB;
			if (___FormatTypes::UnsignedInt8ABGRPACK32 == _formatType)
				return UnsignedInt8RGB;
			if (___FormatTypes::SignedInt8ABGRPACK32 == _formatType)
				return SignedInt8RGB;
			if (___FormatTypes::Srgb8ABGRPACK32 == _formatType)
				return Srgb8RGB;
			if (___FormatTypes::UnsignedNormalized16R == _formatType)
				return UnsignedNormalized16RGB;
			if (___FormatTypes::SignedNormalized16R == _formatType)
				return SignedNormalized16RGB;
			if (___FormatTypes::UnsignedScaled16R == _formatType)
				return UnsignedScaled16RGB;
			if (___FormatTypes::SignedScaled16R == _formatType)
				return SignedScaled16RGB;
			if (___FormatTypes::UnsignedInt16R == _formatType)
				return UnsignedInt16RGB;
			if (___FormatTypes::SignedInt16R == _formatType)
				return SignedInt16RGB;
			if (___FormatTypes::SignedFloat16R == _formatType)
				return SignedFloat16RGB;
			if (___FormatTypes::UnsignedNormalized16RG == _formatType)
				return UnsignedNormalized16RGB;
			if (___FormatTypes::SignedNormalized16RG == _formatType)
				return SignedNormalized16RGB;
			if (___FormatTypes::UnsignedScaled16RG == _formatType)
				return UnsignedScaled16RGB;
			if (___FormatTypes::SignedScaled16RG == _formatType)
				return SignedScaled16RGB;
			if (___FormatTypes::UnsignedInt16RG == _formatType)
				return UnsignedInt16RGB;
			if (___FormatTypes::SignedInt16RG == _formatType)
				return SignedInt16RGB;
			if (___FormatTypes::SignedFloat16RG == _formatType)
				return SignedFloat16RGB;
			if (___FormatTypes::UnsignedNormalized16RGB == _formatType)
				return UnsignedNormalized16RGB;
			if (___FormatTypes::SignedNormalized16RGB == _formatType)
				return SignedNormalized16RGB;
			if (___FormatTypes::UnsignedScaled16RGB == _formatType)
				return UnsignedScaled16RGB;
			if (___FormatTypes::SignedScaled16RGB == _formatType)
				return SignedScaled16RGB;
			if (___FormatTypes::UnsignedInt16RGB == _formatType)
				return UnsignedInt16RGB;
			if (___FormatTypes::SignedInt16RGB == _formatType)
				return SignedInt16RGB;
			if (___FormatTypes::SignedFloat16RGB == _formatType)
				return SignedFloat16RGB;
			if (___FormatTypes::UnsignedNormalized16RGBA == _formatType)
				return UnsignedNormalized16RGB;
			if (___FormatTypes::SignedNormalized16RGBA == _formatType)
				return SignedNormalized16RGB;
			if (___FormatTypes::UnsignedScaled16RGBA == _formatType)
				return UnsignedScaled16RGB;
			if (___FormatTypes::SignedScaled16RGBA == _formatType)
				return SignedScaled16RGB;
			if (___FormatTypes::UnsignedInt16RGBA == _formatType)
				return UnsignedInt16RGB;
			if (___FormatTypes::SignedInt16RGBA == _formatType)
				return SignedInt16RGB;
			if (___FormatTypes::SignedFloat16RGBA == _formatType)
				return SignedFloat16RGB;
			if (___FormatTypes::UnsignedInt32R == _formatType)
				return UnsignedInt32RGB;
			if (___FormatTypes::SignedInt32R == _formatType)
				return SignedInt32RGB;
			if (___FormatTypes::SignedFloat32R == _formatType)
				return SignedFloat32RGB;
			if (___FormatTypes::UnsignedInt32RG == _formatType)
				return UnsignedInt32RGB;
			if (___FormatTypes::SignedInt32RG == _formatType)
				return SignedInt32RGB;
			if (___FormatTypes::SignedFloat32RG == _formatType)
				return SignedFloat32RGB;
			if (___FormatTypes::UnsignedInt32RGB == _formatType)
				return UnsignedInt32RGB;
			if (___FormatTypes::SignedInt32RGB == _formatType)
				return SignedInt32RGB;
			if (___FormatTypes::SignedFloat32RGB == _formatType)
				return SignedFloat32RGB;
			if (___FormatTypes::UnsignedInt32RGBA == _formatType)
				return UnsignedInt32RGB;
			if (___FormatTypes::SignedInt32RGBA == _formatType)
				return SignedInt32RGB;
			if (___FormatTypes::SignedFloat32RGBA == _formatType)
				return SignedFloat32RGB;
			if (___FormatTypes::UnsignedInt64R == _formatType)
				return UnsignedInt64RGB;
			if (___FormatTypes::SignedInt64R == _formatType)
				return SignedInt64RGB;
			if (___FormatTypes::SignedFloat64R == _formatType)
				return SignedFloat64RGB;
			if (___FormatTypes::UnsignedInt64RG == _formatType)
				return UnsignedInt64RGB;
			if (___FormatTypes::SignedInt64RG == _formatType)
				return SignedInt64RGB;
			if (___FormatTypes::SignedFloat64RG == _formatType)
				return SignedFloat64RGB;
			if (___FormatTypes::UnsignedInt64RGB == _formatType)
				return UnsignedInt64RGB;
			if (___FormatTypes::SignedInt64RGB == _formatType)
				return SignedInt64RGB;
			if (___FormatTypes::SignedFloat64RGB == _formatType)
				return SignedFloat64RGB;
			if (___FormatTypes::UnsignedInt64RGBA == _formatType)
				return UnsignedInt64RGB;
			if (___FormatTypes::SignedInt64RGBA == _formatType)
				return SignedInt64RGB;
			if (___FormatTypes::SignedFloat64RGBA == _formatType)
				return SignedFloat64RGB;
		}
		if constexpr (Length == 4) {
			if (___FormatTypes::UnsignedNormalized8R == _formatType)
				return UnsignedNormalized8RGBA;
			if (___FormatTypes::SignedNormalized8R == _formatType)
				return SignedNormalized8RGBA;
			if (___FormatTypes::UnsignedScaled8R == _formatType)
				return UnsignedScaled8RGBA;
			if (___FormatTypes::SignedScaled8R == _formatType)
				return SignedScaled8RGBA;
			if (___FormatTypes::UnsignedInt8R == _formatType)
				return UnsignedInt8RGBA;
			if (___FormatTypes::SignedInt8R == _formatType)
				return SignedInt8RGBA;
			if (___FormatTypes::Srgb8R == _formatType)
				return Srgb8RGBA;
			if (___FormatTypes::UnsignedNormalized8RG == _formatType)
				return UnsignedNormalized8RGBA;
			if (___FormatTypes::SignedNormalized8RG == _formatType)
				return SignedNormalized8RGBA;
			if (___FormatTypes::UnsignedScaled8RG == _formatType)
				return UnsignedScaled8RGBA;
			if (___FormatTypes::SignedScaled8RG == _formatType)
				return SignedScaled8RGBA;
			if (___FormatTypes::UnsignedInt8RG == _formatType)
				return UnsignedInt8RGBA;
			if (___FormatTypes::SignedInt8RG == _formatType)
				return SignedInt8RGBA;
			if (___FormatTypes::Srgb8RG == _formatType)
				return Srgb8RGBA;
			if (___FormatTypes::UnsignedNormalized8RGB == _formatType)
				return UnsignedNormalized8RGBA;
			if (___FormatTypes::SignedNormalized8RGB == _formatType)
				return SignedNormalized8RGBA;
			if (___FormatTypes::UnsignedScaled8RGB == _formatType)
				return UnsignedScaled8RGBA;
			if (___FormatTypes::SignedScaled8RGB == _formatType)
				return SignedScaled8RGBA;
			if (___FormatTypes::UnsignedInt8RGB == _formatType)
				return UnsignedInt8RGBA;
			if (___FormatTypes::SignedInt8RGB == _formatType)
				return SignedInt8RGBA;
			if (___FormatTypes::Srgb8RGB == _formatType)
				return Srgb8RGBA;
			if (___FormatTypes::UnsignedNormalized8BGR == _formatType)
				return UnsignedNormalized8RGBA;
			if (___FormatTypes::SignedNormalized8BGR == _formatType)
				return SignedNormalized8RGBA;
			if (___FormatTypes::UnsignedScaled8BGR == _formatType)
				return UnsignedScaled8RGBA;
			if (___FormatTypes::SignedScaled8BGR == _formatType)
				return SignedScaled8RGBA;
			if (___FormatTypes::UnsignedInt8BGR == _formatType)
				return UnsignedInt8RGBA;
			if (___FormatTypes::SignedInt8BGR == _formatType)
				return SignedInt8RGBA;
			if (___FormatTypes::Srgb8BGR == _formatType)
				return Srgb8RGBA;
			if (___FormatTypes::UnsignedNormalized8RGBA == _formatType)
				return UnsignedNormalized8RGBA;
			if (___FormatTypes::SignedNormalized8RGBA == _formatType)
				return SignedNormalized8RGBA;
			if (___FormatTypes::UnsignedScaled8RGBA == _formatType)
				return UnsignedScaled8RGBA;
			if (___FormatTypes::SignedScaled8RGBA == _formatType)
				return SignedScaled8RGBA;
			if (___FormatTypes::UnsignedInt8RGBA == _formatType)
				return UnsignedInt8RGBA;
			if (___FormatTypes::SignedInt8RGBA == _formatType)
				return SignedInt8RGBA;
			if (___FormatTypes::Srgb8RGBA == _formatType)
				return Srgb8RGBA;
			if (___FormatTypes::UnsignedNormalized8BGRA == _formatType)
				return UnsignedNormalized8RGBA;
			if (___FormatTypes::SignedNormalized8BGRA == _formatType)
				return SignedNormalized8RGBA;
			if (___FormatTypes::UnsignedScaled8BGRA == _formatType)
				return UnsignedScaled8RGBA;
			if (___FormatTypes::SignedScaled8BGRA == _formatType)
				return SignedScaled8RGBA;
			if (___FormatTypes::UnsignedInt8BGRA == _formatType)
				return UnsignedInt8RGBA;
			if (___FormatTypes::SignedInt8BGRA == _formatType)
				return SignedInt8RGBA;
			if (___FormatTypes::Srgb8BGRA == _formatType)
				return Srgb8RGBA;
			if (___FormatTypes::UnsignedNormalized8ABGRPACK32 == _formatType)
				return UnsignedNormalized8ABGRPACK32;
			if (___FormatTypes::SignedNormalized8ABGRPACK32 == _formatType)
				return SignedNormalized8ABGRPACK32;
			if (___FormatTypes::UnsignedScaled8ABGRPACK32 == _formatType)
				return UnsignedScaled8ABGRPACK32;
			if (___FormatTypes::SignedScaled8ABGRPACK32 == _formatType)
				return SignedScaled8ABGRPACK32;
			if (___FormatTypes::UnsignedInt8ABGRPACK32 == _formatType)
				return UnsignedInt8ABGRPACK32;
			if (___FormatTypes::SignedInt8ABGRPACK32 == _formatType)
				return SignedInt8ABGRPACK32;
			if (___FormatTypes::Srgb8ABGRPACK32 == _formatType)
				return Srgb8ABGRPACK32;
			if (___FormatTypes::UnsignedNormalized16R == _formatType)
				return UnsignedNormalized16RGBA;
			if (___FormatTypes::SignedNormalized16R == _formatType)
				return SignedNormalized16RGBA;
			if (___FormatTypes::UnsignedScaled16R == _formatType)
				return UnsignedScaled16RGBA;
			if (___FormatTypes::SignedScaled16R == _formatType)
				return SignedScaled16RGBA;
			if (___FormatTypes::UnsignedInt16R == _formatType)
				return UnsignedInt16RGBA;
			if (___FormatTypes::SignedInt16R == _formatType)
				return SignedInt16RGBA;
			if (___FormatTypes::SignedFloat16R == _formatType)
				return SignedFloat16RGBA;
			if (___FormatTypes::UnsignedNormalized16RG == _formatType)
				return UnsignedNormalized16RGBA;
			if (___FormatTypes::SignedNormalized16RG == _formatType)
				return SignedNormalized16RGBA;
			if (___FormatTypes::UnsignedScaled16RG == _formatType)
				return UnsignedScaled16RGBA;
			if (___FormatTypes::SignedScaled16RG == _formatType)
				return SignedScaled16RGBA;
			if (___FormatTypes::UnsignedInt16RG == _formatType)
				return UnsignedInt16RGBA;
			if (___FormatTypes::SignedInt16RG == _formatType)
				return SignedInt16RGBA;
			if (___FormatTypes::SignedFloat16RG == _formatType)
				return SignedFloat16RGBA;
			if (___FormatTypes::UnsignedNormalized16RGB == _formatType)
				return UnsignedNormalized16RGBA;
			if (___FormatTypes::SignedNormalized16RGB == _formatType)
				return SignedNormalized16RGBA;
			if (___FormatTypes::UnsignedScaled16RGB == _formatType)
				return UnsignedScaled16RGBA;
			if (___FormatTypes::SignedScaled16RGB == _formatType)
				return SignedScaled16RGBA;
			if (___FormatTypes::UnsignedInt16RGB == _formatType)
				return UnsignedInt16RGBA;
			if (___FormatTypes::SignedInt16RGB == _formatType)
				return SignedInt16RGBA;
			if (___FormatTypes::SignedFloat16RGB == _formatType)
				return SignedFloat16RGBA;
			if (___FormatTypes::UnsignedNormalized16RGBA == _formatType)
				return UnsignedNormalized16RGBA;
			if (___FormatTypes::SignedNormalized16RGBA == _formatType)
				return SignedNormalized16RGBA;
			if (___FormatTypes::UnsignedScaled16RGBA == _formatType)
				return UnsignedScaled16RGBA;
			if (___FormatTypes::SignedScaled16RGBA == _formatType)
				return SignedScaled16RGBA;
			if (___FormatTypes::UnsignedInt16RGBA == _formatType)
				return UnsignedInt16RGBA;
			if (___FormatTypes::SignedInt16RGBA == _formatType)
				return SignedInt16RGBA;
			if (___FormatTypes::SignedFloat16RGBA == _formatType)
				return SignedFloat16RGBA;
			if (___FormatTypes::UnsignedInt32R == _formatType)
				return UnsignedInt32RGBA;
			if (___FormatTypes::SignedInt32R == _formatType)
				return SignedInt32RGBA;
			if (___FormatTypes::SignedFloat32R == _formatType)
				return SignedFloat32RGBA;
			if (___FormatTypes::UnsignedInt32RG == _formatType)
				return UnsignedInt32RGBA;
			if (___FormatTypes::SignedInt32RG == _formatType)
				return SignedInt32RGBA;
			if (___FormatTypes::SignedFloat32RG == _formatType)
				return SignedFloat32RGBA;
			if (___FormatTypes::UnsignedInt32RGB == _formatType)
				return UnsignedInt32RGBA;
			if (___FormatTypes::SignedInt32RGB == _formatType)
				return SignedInt32RGBA;
			if (___FormatTypes::SignedFloat32RGB == _formatType)
				return SignedFloat32RGBA;
			if (___FormatTypes::UnsignedInt32RGBA == _formatType)
				return UnsignedInt32RGBA;
			if (___FormatTypes::SignedInt32RGBA == _formatType)
				return SignedInt32RGBA;
			if (___FormatTypes::SignedFloat32RGBA == _formatType)
				return SignedFloat32RGBA;
			if (___FormatTypes::UnsignedInt64R == _formatType)
				return UnsignedInt64RGBA;
			if (___FormatTypes::SignedInt64R == _formatType)
				return SignedInt64RGBA;
			if (___FormatTypes::SignedFloat64R == _formatType)
				return SignedFloat64RGBA;
			if (___FormatTypes::UnsignedInt64RG == _formatType)
				return UnsignedInt64RGBA;
			if (___FormatTypes::SignedInt64RG == _formatType)
				return SignedInt64RGBA;
			if (___FormatTypes::SignedFloat64RG == _formatType)
				return SignedFloat64RGBA;
			if (___FormatTypes::UnsignedInt64RGB == _formatType)
				return UnsignedInt64RGBA;
			if (___FormatTypes::SignedInt64RGB == _formatType)
				return SignedInt64RGBA;
			if (___FormatTypes::SignedFloat64RGB == _formatType)
				return SignedFloat64RGBA;
			if (___FormatTypes::UnsignedInt64RGBA == _formatType)
				return UnsignedInt64RGBA;
			if (___FormatTypes::SignedInt64RGBA == _formatType)
				return SignedInt64RGBA;
			if (___FormatTypes::SignedFloat64RGBA == _formatType)
				return SignedFloat64RGBA;
		}
		return Undefined;
	}

	inline constexpr size_t ___Format::getVecLength() const {
		if (___FormatTypes::UnsignedNormalized8R == _formatType)
			return 1;
		if (___FormatTypes::SignedNormalized8R == _formatType)
			return 1;
		if (___FormatTypes::UnsignedScaled8R == _formatType)
			return 1;
		if (___FormatTypes::SignedScaled8R == _formatType)
			return 1;
		if (___FormatTypes::UnsignedInt8R == _formatType)
			return 1;
		if (___FormatTypes::SignedInt8R == _formatType)
			return 1;
		if (___FormatTypes::Srgb8R == _formatType)
			return 1;
		if (___FormatTypes::UnsignedNormalized8RG == _formatType)
			return 2;
		if (___FormatTypes::SignedNormalized8RG == _formatType)
			return 2;
		if (___FormatTypes::UnsignedScaled8RG == _formatType)
			return 2;
		if (___FormatTypes::SignedScaled8RG == _formatType)
			return 2;
		if (___FormatTypes::UnsignedInt8RG == _formatType)
			return 2;
		if (___FormatTypes::SignedInt8RG == _formatType)
			return 2;
		if (___FormatTypes::Srgb8RG == _formatType)
			return 2;
		if (___FormatTypes::UnsignedNormalized8RGB == _formatType)
			return 3;
		if (___FormatTypes::SignedNormalized8RGB == _formatType)
			return 3;
		if (___FormatTypes::UnsignedScaled8RGB == _formatType)
			return 3;
		if (___FormatTypes::SignedScaled8RGB == _formatType)
			return 3;
		if (___FormatTypes::UnsignedInt8RGB == _formatType)
			return 3;
		if (___FormatTypes::SignedInt8RGB == _formatType)
			return 3;
		if (___FormatTypes::Srgb8RGB == _formatType)
			return 3;
		if (___FormatTypes::UnsignedNormalized8BGR == _formatType)
			return 3;
		if (___FormatTypes::SignedNormalized8BGR == _formatType)
			return 3;
		if (___FormatTypes::UnsignedScaled8BGR == _formatType)
			return 3;
		if (___FormatTypes::SignedScaled8BGR == _formatType)
			return 3;
		if (___FormatTypes::UnsignedInt8BGR == _formatType)
			return 3;
		if (___FormatTypes::SignedInt8BGR == _formatType)
			return 3;
		if (___FormatTypes::Srgb8BGR == _formatType)
			return 3;
		if (___FormatTypes::UnsignedNormalized8RGBA == _formatType)
			return 4;
		if (___FormatTypes::SignedNormalized8RGBA == _formatType)
			return 4;
		if (___FormatTypes::UnsignedScaled8RGBA == _formatType)
			return 4;
		if (___FormatTypes::SignedScaled8RGBA == _formatType)
			return 4;
		if (___FormatTypes::UnsignedInt8RGBA == _formatType)
			return 4;
		if (___FormatTypes::SignedInt8RGBA == _formatType)
			return 4;
		if (___FormatTypes::Srgb8RGBA == _formatType)
			return 4;
		if (___FormatTypes::UnsignedNormalized8BGRA == _formatType)
			return 4;
		if (___FormatTypes::SignedNormalized8BGRA == _formatType)
			return 4;
		if (___FormatTypes::UnsignedScaled8BGRA == _formatType)
			return 4;
		if (___FormatTypes::SignedScaled8BGRA == _formatType)
			return 4;
		if (___FormatTypes::UnsignedInt8BGRA == _formatType)
			return 4;
		if (___FormatTypes::SignedInt8BGRA == _formatType)
			return 4;
		if (___FormatTypes::Srgb8BGRA == _formatType)
			return 4;
		if (___FormatTypes::UnsignedNormalized8ABGRPACK32 == _formatType)
			return 4;
		if (___FormatTypes::SignedNormalized8ABGRPACK32 == _formatType)
			return 4;
		if (___FormatTypes::UnsignedScaled8ABGRPACK32 == _formatType)
			return 4;
		if (___FormatTypes::SignedScaled8ABGRPACK32 == _formatType)
			return 4;
		if (___FormatTypes::UnsignedInt8ABGRPACK32 == _formatType)
			return 4;
		if (___FormatTypes::SignedInt8ABGRPACK32 == _formatType)
			return 4;
		if (___FormatTypes::Srgb8ABGRPACK32 == _formatType)
			return 4;
		if (___FormatTypes::UnsignedNormalized16R == _formatType)
			return 1;
		if (___FormatTypes::SignedNormalized16R == _formatType)
			return 1;
		if (___FormatTypes::UnsignedScaled16R == _formatType)
			return 1;
		if (___FormatTypes::SignedScaled16R == _formatType)
			return 1;
		if (___FormatTypes::UnsignedInt16R == _formatType)
			return 1;
		if (___FormatTypes::SignedInt16R == _formatType)
			return 1;
		if (___FormatTypes::SignedFloat16R == _formatType)
			return 1;
		if (___FormatTypes::UnsignedNormalized16RG == _formatType)
			return 2;
		if (___FormatTypes::SignedNormalized16RG == _formatType)
			return 2;
		if (___FormatTypes::UnsignedScaled16RG == _formatType)
			return 2;
		if (___FormatTypes::SignedScaled16RG == _formatType)
			return 2;
		if (___FormatTypes::UnsignedInt16RG == _formatType)
			return 2;
		if (___FormatTypes::SignedInt16RG == _formatType)
			return 2;
		if (___FormatTypes::SignedFloat16RG == _formatType)
			return 2;
		if (___FormatTypes::UnsignedNormalized16RGB == _formatType)
			return 3;
		if (___FormatTypes::SignedNormalized16RGB == _formatType)
			return 3;
		if (___FormatTypes::UnsignedScaled16RGB == _formatType)
			return 3;
		if (___FormatTypes::SignedScaled16RGB == _formatType)
			return 3;
		if (___FormatTypes::UnsignedInt16RGB == _formatType)
			return 3;
		if (___FormatTypes::SignedInt16RGB == _formatType)
			return 3;
		if (___FormatTypes::SignedFloat16RGB == _formatType)
			return 3;
		if (___FormatTypes::UnsignedNormalized16RGBA == _formatType)
			return 4;
		if (___FormatTypes::SignedNormalized16RGBA == _formatType)
			return 4;
		if (___FormatTypes::UnsignedScaled16RGBA == _formatType)
			return 4;
		if (___FormatTypes::SignedScaled16RGBA == _formatType)
			return 4;
		if (___FormatTypes::UnsignedInt16RGBA == _formatType)
			return 4;
		if (___FormatTypes::SignedInt16RGBA == _formatType)
			return 4;
		if (___FormatTypes::SignedFloat16RGBA == _formatType)
			return 4;
		if (___FormatTypes::UnsignedInt32R == _formatType)
			return 1;
		if (___FormatTypes::SignedInt32R == _formatType)
			return 1;
		if (___FormatTypes::SignedFloat32R == _formatType)
			return 1;
		if (___FormatTypes::UnsignedInt32RG == _formatType)
			return 2;
		if (___FormatTypes::SignedInt32RG == _formatType)
			return 2;
		if (___FormatTypes::SignedFloat32RG == _formatType)
			return 2;
		if (___FormatTypes::UnsignedInt32RGB == _formatType)
			return 3;
		if (___FormatTypes::SignedInt32RGB == _formatType)
			return 3;
		if (___FormatTypes::SignedFloat32RGB == _formatType)
			return 3;
		if (___FormatTypes::UnsignedInt32RGBA == _formatType)
			return 4;
		if (___FormatTypes::SignedInt32RGBA == _formatType)
			return 4;
		if (___FormatTypes::SignedFloat32RGBA == _formatType)
			return 4;
		if (___FormatTypes::UnsignedInt64R == _formatType)
			return 1;
		if (___FormatTypes::SignedInt64R == _formatType)
			return 1;
		if (___FormatTypes::SignedFloat64R == _formatType)
			return 1;
		if (___FormatTypes::UnsignedInt64RG == _formatType)
			return 2;
		if (___FormatTypes::SignedInt64RG == _formatType)
			return 2;
		if (___FormatTypes::SignedFloat64RG == _formatType)
			return 2;
		if (___FormatTypes::UnsignedInt64RGB == _formatType)
			return 3;
		if (___FormatTypes::SignedInt64RGB == _formatType)
			return 3;
		if (___FormatTypes::SignedFloat64RGB == _formatType)
			return 3;
		if (___FormatTypes::UnsignedInt64RGBA == _formatType)
			return 4;
		if (___FormatTypes::SignedInt64RGBA == _formatType)
			return 4;
		if (___FormatTypes::SignedFloat64RGBA == _formatType)
			return 4;
		return 0;
	}
}