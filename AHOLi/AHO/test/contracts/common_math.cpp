#include <AHO/core/math/quaternion.hpp>
#include <AHO/core/math/projection.hpp>

#ifdef GLM_VERSION
#error AHO math must not include GLM
#endif

namespace {
    std::array<float, 16> model_frame(float amount);
    std::array<float, 16> overlay_frame();

    std::array<float, 16> model_frame(float amount) {
        const auto target = aho::UnitQuaternionF::FromAxisAngle(aho::Degree{90}, aho::d3::VectorF{0, 1, 0});
        const auto rotation = aho::math::slerp(aho::UnitQuaternionF{}, target, amount);
        return aho::matrix::to_array<aho::MatrixLayout::COLUMN_MAJOR>(aho::matrix::make_trs(
            aho::d3::VectorF{10, 0, 0}, rotation, aho::d3::VectorF{1, 2, 1}));
    }

    std::array<float, 16> overlay_frame() {
        return aho::matrix::to_array<aho::MatrixLayout::COLUMN_MAJOR>(aho::matrix::make_orthographic<aho::HandedCoordinate::RIGHT, aho::DepthRange::ZERO_TO_ONE, aho::ClipY::DOWN>(
            0.f, 800.f, 0.f, 600.f, 0.f, 100.f));
    }
} // namespace
