#include <AHO/x/scene/camera.hpp>
#include <gtest/gtest.h>
#include <limits>

namespace {
    std::array<float, 4> transform(const aho::Mat4x4<float>& m, std::array<float, 4> p) {
        std::array<float, 4> out{};
        for (int row = 0; row < 4; ++row)
            for (int column = 0; column < 4; ++column)
                out[row] += m[column][row] * p[column];
        return out;
    }
    TEST(SceneCameraTest, MapsNearAndFarToVulkanDepthRange) {
        aho::x::scene::Camera camera;
        auto near = transform(camera.projection(), {0, 0, -camera.nearPlane, 1});
        auto far = transform(camera.projection(), {0, 0, -camera.farPlane, 1});
        EXPECT_NEAR(near[2] / near[3], 0, .00001f);
        EXPECT_NEAR(far[2] / far[3], 1, .00001f);
    }
    TEST(SceneCameraTest, PositiveWorldYAppearsAboveCenterAndPositiveXToTheRight) {
        aho::x::scene::Camera camera;
        auto p = transform(camera.viewProjection(), {1, 1, -10, 1});
        EXPECT_GT(p[0] / p[3], 0);
        EXPECT_LT(p[1] / p[3], 0);
    }
    TEST(SceneCameraTest, TranslatedRotatedCameraCentersItsTarget) {
        aho::x::scene::Camera camera;
        camera.eye = {10, 5, 8};
        camera.direction = {2, 1, -3};
        auto p = transform(camera.viewProjection(), {12, 6, 5, 1});
        EXPECT_NEAR(p[0] / p[3], 0, .00001f);
        EXPECT_NEAR(p[1] / p[3], 0, .00001f);
        EXPECT_GT(p[2] / p[3], 0);
        EXPECT_LT(p[2] / p[3], 1);
    }
    TEST(SceneCameraTest, RejectsInvalidProjectionAndParallelDirection) {
        aho::x::scene::Camera camera;
        camera.aspect = 0;
        EXPECT_THROW(camera.projection(), std::invalid_argument);
        camera.aspect = 1;
        camera.farPlane = camera.nearPlane;
        EXPECT_THROW(camera.projection(), std::invalid_argument);
        camera.direction = {0, 1, 0};
        EXPECT_THROW(camera.view(), std::invalid_argument);
        camera.direction = {0, 0, 0};
        EXPECT_THROW(camera.view(), std::invalid_argument);
        camera.eye = {std::numeric_limits<float>::infinity(), 0, 0};
        camera.direction = {0, 0, -1};
        EXPECT_THROW(camera.view(), std::invalid_argument);
    }
} // namespace
