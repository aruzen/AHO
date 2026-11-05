#include "support/render_fixture.hpp"
#include <cmath>
#include <limits>
namespace {
    TEST(ScissorTest, ConstructorsPreserveRectangleCoordinates) {
        vsl::Scissor defaults;
        EXPECT_EQ(defaults.x, 0);
        EXPECT_EQ(defaults.y, 0);
        EXPECT_EQ(defaults.width, 1u);
        EXPECT_EQ(defaults.height, 1u);
        float x = 2, y = 3, width = 4, height = 5;
        vsl::VSLD2RectangleAccessor<float> rect{{x, y}, {width, height}};
        vsl::Scissor direct(x, y, width, height), adapted(rect);
        EXPECT_EQ(direct.x, 2);
        EXPECT_EQ(direct.y, 3);
        EXPECT_EQ(direct.width, 4u);
        EXPECT_EQ(direct.height, 5u);
        EXPECT_EQ(adapted.x, 2);
        EXPECT_EQ(adapted.y, 3);
        EXPECT_EQ(adapted.width, 4u);
        EXPECT_EQ(adapted.height, 5u);
    }
    TEST(ScissorTest, NonFiniteNegativeAndUnrepresentableFloatComponentsAreRejected) {
        const auto infinity = std::numeric_limits<float>::infinity();
        const auto nan = std::numeric_limits<float>::quiet_NaN();
        for (const auto invalid : {-1.f, infinity, -infinity, nan, static_cast<float>(INT32_MAX), static_cast<float>(UINT32_MAX)}) {
            for (std::size_t component = 0; component < 4; ++component) {
                std::array values{0.f, 0.f, 1.f, 1.f};
                values[component] = invalid;
                EXPECT_THROW((vsl::Scissor(values[0], values[1], values[2], values[3])), std::invalid_argument);
                vsl::VSLD2RectangleAccessor<float> rect{{values[0], values[1]}, {values[2], values[3]}};
                EXPECT_THROW((vsl::Scissor(rect)), std::invalid_argument);
            }
        }
    }
    TEST(ScissorTest, IntegerEndpointsMustFitWithoutSignedOverflow) {
        const auto extent = std::nextafter(static_cast<float>(INT32_MAX), 0.f);
        EXPECT_THROW((vsl::Scissor(128, 0, extent, 1)), std::invalid_argument);
        EXPECT_THROW((vsl::Scissor(0, 128, 1, extent)), std::invalid_argument);
        const vsl::Scissor boundary(127, 127, extent, extent);
        EXPECT_EQ(std::uint64_t(boundary.x) + boundary.width, INT32_MAX);
        EXPECT_EQ(std::uint64_t(boundary.y) + boundary.height, INT32_MAX);
        const vsl::Scissor fractional(2.75f, 3.5f, 4.75f, 5.5f);
        EXPECT_EQ(fractional.x, 2);
        EXPECT_EQ(fractional.y, 3);
        EXPECT_EQ(fractional.width, 4u);
        EXPECT_EQ(fractional.height, 5u);
        const vsl::Scissor empty(-0.f, 0, 0, 0);
        EXPECT_EQ(empty.width, 0u);
        EXPECT_EQ(empty.height, 0u);
        const vsl::Scissor truncated(-.5f, -.5f, -.5f, -.5f);
        EXPECT_EQ(truncated.x, 0);
        EXPECT_EQ(truncated.y, 0);
        EXPECT_EQ(truncated.width, 0u);
        EXPECT_EQ(truncated.height, 0u);
    }
    using ScissorVulkanIntegrationTest = vsl_test::RenderVulkanIntegrationTest;
    class ScissorValidationVulkanIntegrationTest : public vsl_test::RenderVulkanIntegrationTest {
        bool validate_vulkan() const override { return true; }
    };
    TEST_F(ScissorValidationVulkanIntegrationTest, MutatedInvalidRectanglesAreRejectedBeforeInjectionAndRecording) {
        expect_validation_abort("");
        auto manager = make_manager();
        auto phase = manager.start_phase<vsl::ComputePhase>();
        for (std::size_t invalid = 0; invalid < 6; ++invalid) {
            vsl::Scissor scissor;
            switch (invalid) {
                case 0:
                    scissor.x = -1;
                    break;
                case 1:
                    scissor.y = -1;
                    break;
                case 2:
                    scissor.width = UINT32_MAX;
                    break;
                case 3:
                    scissor.height = UINT32_MAX;
                    break;
                case 4:
                    scissor.x = 1;
                    scissor.width = INT32_MAX;
                    break;
                case 5:
                    scissor.y = 1;
                    scissor.height = INT32_MAX;
                    break;
            }
            auto state = make_layout_state();
            EXPECT_THROW(scissor.injection(state), std::invalid_argument);
            EXPECT_TRUE(state.data_->info->scissors.empty());
            EXPECT_THROW(phase << scissor, std::invalid_argument);
        }
        phase << vsl::Scissor(0, 0, 0, 0);
        EXPECT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
        EXPECT_TRUE(phase.completion().wait());
        ExpectTriangleColor(render([&](auto& stream) { stream << vsl::Scissor(description) << vsl::command::Draw{3}; }));
    }
    TEST_F(ScissorVulkanIntegrationTest, ScissorExcludesCenterPixel) {
        vsl::Scissor scissor(0, 0, 1, 1);
        auto state = make_layout_state();
        scissor.injection(state);
        ASSERT_EQ(state.data_->info->scissors.size(), 1u);
        EXPECT_EQ(state.data_->info->scissors[0].extent, (vk::Extent2D{1, 1}));
        vsl::Scissor all(description);
        EXPECT_EQ(all.width, 8u);
        EXPECT_EQ(all.height, 8u);
        EXPECT_EQ(render([&](auto& stream) { stream << scissor << vsl::command::Draw{3}; }),
                  (std::array<std::uint8_t, 4>{0, 0, 0, 0}));
    }
} // namespace
