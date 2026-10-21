#include "support/render_fixture.hpp"
namespace {
    class DrawVulkanIntegrationTest : public vsl_test::RenderVulkanIntegrationTest {
        bool validate_vulkan() const override { return true; }
    };
    TEST(DrawTest, ConstructorsAndManipulatorKeepVertexCount) {
        EXPECT_EQ(vsl::command::Draw{}.get_size(), 0u);
        EXPECT_EQ(vsl::command::Draw{3}.get_size(), 3u);
        vsl::command::DrawManip draw;
        draw.set_vertex_size(3);
        ASSERT_TRUE(draw.get_size().has_value());
        EXPECT_EQ(*draw.get_size(), 3u);
        draw.set_vertex_size(std::nullopt);
        EXPECT_FALSE(draw.get_size().has_value());
    }
    TEST_F(DrawVulkanIntegrationTest, DrawsTriangleWithKnownPixelColor) {
        ExpectTriangleColor(render([](auto& stream) { stream << vsl::command::Draw{3}; }));
    }
    TEST_F(DrawVulkanIntegrationTest, SharedManipulatorDrawsRequestedVertexCount) {
        auto draw = std::make_shared<vsl::command::DrawManip>();
        draw->set_vertex_size(3);
        ExpectTriangleColor(render([&](auto& stream) {
            stream.vertex_size = 3;
            stream << std::static_pointer_cast<vsl::command::Command>(draw);
        }));
    }
} // namespace

#include "../../../src/vulkan/commands/draw_count.hpp"
#include <limits>
namespace {
    static_assert(vsl::impl::helper::checked_draw_count(0) == 0);
    static_assert(vsl::impl::helper::checked_draw_count(std::numeric_limits<std::uint32_t>::max()) == std::numeric_limits<std::uint32_t>::max());
    TEST(DrawCountTest, FullUint32RangeIsPreserved) {
        for (const auto count : {std::uint32_t{0}, std::uint32_t{1}, std::numeric_limits<std::uint32_t>::max()})
            EXPECT_EQ(vsl::impl::helper::checked_draw_count(count), count);
    }
    TEST(DrawCountTest, OversizedCountsReportTheNativeLimit) {
        if constexpr (sizeof(std::size_t) <= sizeof(std::uint32_t)) {
            GTEST_SKIP() << "size_t has no values above uint32_t";
        } else {
            const auto overflow = std::size_t{std::numeric_limits<std::uint32_t>::max()} + 1;
            for (const auto count : {overflow, std::numeric_limits<std::size_t>::max()}) {
                try {
                    (void)vsl::impl::helper::checked_draw_count(count);
                    ADD_FAILURE() << "Oversized draw count was accepted";
                } catch (const vsl::exceptions::RuntimeException& error) {
                    EXPECT_NE(error.reason.find("uint32"), std::string::npos);
                }
            }
        }
    }
    TEST_F(DrawVulkanIntegrationTest, OversizedDrawAndManipulatorCountsAreRejectedBeforeRecording) {
        if constexpr (sizeof(std::size_t) <= sizeof(std::uint32_t)) {
            GTEST_SKIP() << "size_t has no values above uint32_t";
        } else {
            const auto overflow = std::size_t{std::numeric_limits<std::uint32_t>::max()} + 1;
            ExpectTriangleColor(render([&](auto& stream) {
                EXPECT_THROW(stream << vsl::command::Draw(overflow), vsl::exceptions::RuntimeException);
                stream.vertex_size = overflow;
                EXPECT_THROW(stream << vsl::command::DRAW, vsl::exceptions::RuntimeException);
                stream.vertex_size = 3;
                stream << vsl::command::DRAW;
            }));
        }
    }
} // namespace

namespace {
    TEST(DrawTest, ConstructorsAndSettersKeepTheFullNativeCountRange) {
        for (const auto count : {std::size_t{0}, std::size_t{1}, std::size_t{std::numeric_limits<std::uint32_t>::max()}}) {
            vsl::command::Draw draw(count);
            EXPECT_EQ(draw.get_size(), count);
            draw.set_size(7);
            draw.set_size(count);
            EXPECT_EQ(draw.get_size(), count);
            vsl::command::DrawManip manip;
            manip.set_vertex_size(count);
            ASSERT_TRUE(manip.get_size());
            EXPECT_EQ(*manip.get_size(), count);
        }
    }
    TEST(DrawTest, InvalidInputsAreRejectedBeforeRecordingAndKeepThePreviousCount) {
        if constexpr (sizeof(std::size_t) <= sizeof(std::uint32_t)) {
            GTEST_SKIP() << "size_t has no values above uint32_t";
        } else {
            vsl::command::Draw draw(3);
            vsl::command::DrawManip manip;
            const auto overflow = std::size_t{std::numeric_limits<std::uint32_t>::max()} + 1;
            for (const auto count : {overflow, std::numeric_limits<std::size_t>::max()}) {
                EXPECT_THROW((vsl::command::Draw(count)), vsl::exceptions::RuntimeException);
                EXPECT_THROW(draw.set_size(count), vsl::exceptions::RuntimeException);
                EXPECT_EQ(draw.get_size(), 3u);
                manip.set_vertex_size(std::nullopt);
                EXPECT_THROW(manip.set_vertex_size(count), vsl::exceptions::RuntimeException);
                EXPECT_FALSE(manip.get_size());
                manip.set_vertex_size(3);
                EXPECT_THROW(manip.set_vertex_size(count), vsl::exceptions::RuntimeException);
                EXPECT_EQ(manip.get_size(), 3u);
            }
        }
    }
    TEST_F(DrawVulkanIntegrationTest, SetterValidatedCommandCanBeReusedForMultipleDraws) {
        vsl::command::Draw draw;
        draw.set_size(3);
        ExpectTriangleColor(render([&](auto& stream) { stream << draw << draw; }));
        EXPECT_EQ(draw.get_size(), 3u);
    }
} // namespace
