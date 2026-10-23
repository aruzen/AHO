#pragma once
#include "../../define.hpp"

namespace AHO_NAMESPACE {
    enum class HandedCoordinate {
        LEFT,
        RIGHT
    };
    enum class DepthRange {
        ZERO_TO_ONE,
        NEGATIVE_ONE_TO_ONE
    };
    enum class ClipY {
        UP,
        DOWN
    };
    enum class VectorConvention {
        COLUMN,
        ROW
    };
    enum class MatrixLayout {
        ROW_MAJOR,
        COLUMN_MAJOR
    };
    enum class ViewOrientation {
        UPRIGHT,
        ROLL_180
    };
    enum class InterpolationPath {
        SHORTEST,
        DIRECT
    };

    inline constexpr auto DEFAULT_COORDINATE_SYSTEM = HandedCoordinate::LEFT;
    inline constexpr auto DEFAULT_DEPTH_RANGE = DepthRange::ZERO_TO_ONE;
    inline constexpr auto DEFAULT_CLIP_Y = ClipY::UP;
    inline constexpr auto DEFAULT_VECTOR_CONVENTION = VectorConvention::COLUMN;
    inline constexpr auto DEFAULT_MATRIX_LAYOUT = MatrixLayout::ROW_MAJOR;
    inline constexpr auto DEFAULT_VIEW_ORIENTATION = ViewOrientation::UPRIGHT;
    inline constexpr auto DEFAULT_INTERPOLATION_PATH = InterpolationPath::SHORTEST;
} // namespace AHO_NAMESPACE
