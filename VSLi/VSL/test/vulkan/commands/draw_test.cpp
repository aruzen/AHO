#include "support/render_fixture.hpp"
namespace {
    class DrawVulkanIntegrationTest : public vsl_test::RenderVulkanIntegrationTest {
        bool validate_vulkan() const override { return true; }
    };
    TEST(DrawTest, ConstructorsAndManipulatorKeepVertexCount) {
        EXPECT_EQ(vsl::command::Draw{}.getSize(), 0u);
        EXPECT_EQ(vsl::command::Draw{3}.getSize(), 3u);
        vsl::command::DrawManip draw;
        draw.setVertexSize(3);
        ASSERT_TRUE(draw.getSize().has_value());
        EXPECT_EQ(*draw.getSize(), 3u);
        draw.setVertexSize(std::nullopt);
        EXPECT_FALSE(draw.getSize().has_value());
    }
    TEST_F(DrawVulkanIntegrationTest, DrawsTriangleWithKnownPixelColor) {
        expect_triangle_color(render([](auto& stream) { stream << vsl::command::Draw{3}; }));
    }
    TEST_F(DrawVulkanIntegrationTest, SharedManipulatorDrawsRequestedVertexCount) {
        auto draw = std::make_shared<vsl::command::DrawManip>();
        draw->setVertexSize(3);
        expect_triangle_color(render([&](auto& stream) {
            stream.vertexSize = 3;
            stream << std::static_pointer_cast<vsl::command::__Command>(draw);
        }));
    }
} // namespace

#include "../../../src/vulkan/commands/draw_count.hpp"
#include <limits>
namespace {
    static_assert(vsl::_impl::helper::checkedDrawCount(0) == 0);
    static_assert(vsl::_impl::helper::checkedDrawCount(std::numeric_limits<std::uint32_t>::max()) == std::numeric_limits<std::uint32_t>::max());
    TEST(DrawCountTest, FullUint32RangeIsPreserved) {
        for (const auto count : {std::uint32_t{0}, std::uint32_t{1}, std::numeric_limits<std::uint32_t>::max()})
            EXPECT_EQ(vsl::_impl::helper::checkedDrawCount(count), count);
    }
    TEST(DrawCountTest, OversizedCountsReportTheNativeLimit) {
        if constexpr (sizeof(std::size_t) <= sizeof(std::uint32_t)) {
            GTEST_SKIP() << "size_t has no values above uint32_t";
        } else {
            const auto overflow = std::size_t{std::numeric_limits<std::uint32_t>::max()} + 1;
            for (const auto count : {overflow, std::numeric_limits<std::size_t>::max()}) {
                try {
                    (void)vsl::_impl::helper::checkedDrawCount(count);
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
            expect_triangle_color(render([&](auto& stream) {
                EXPECT_THROW(stream << vsl::command::Draw(overflow), vsl::exceptions::RuntimeException);
                stream.vertexSize = overflow;
                EXPECT_THROW(stream << vsl::command::draw, vsl::exceptions::RuntimeException);
                stream.vertexSize = 3;
                stream << vsl::command::draw;
            }));
        }
    }
} // namespace

namespace {
    TEST(DrawTest, ConstructorsAndSettersKeepTheFullNativeCountRange) {
        for (const auto count : {std::size_t{0}, std::size_t{1}, std::size_t{std::numeric_limits<std::uint32_t>::max()}}) {
            vsl::command::Draw draw(count);
            EXPECT_EQ(draw.getSize(), count);
            draw.setSize(7);
            draw.setSize(count);
            EXPECT_EQ(draw.getSize(), count);
            vsl::command::DrawManip manip;
            manip.setVertexSize(count);
            ASSERT_TRUE(manip.getSize());
            EXPECT_EQ(*manip.getSize(), count);
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
                EXPECT_THROW(draw.setSize(count), vsl::exceptions::RuntimeException);
                EXPECT_EQ(draw.getSize(), 3u);
                manip.setVertexSize(std::nullopt);
                EXPECT_THROW(manip.setVertexSize(count), vsl::exceptions::RuntimeException);
                EXPECT_FALSE(manip.getSize());
                manip.setVertexSize(3);
                EXPECT_THROW(manip.setVertexSize(count), vsl::exceptions::RuntimeException);
                EXPECT_EQ(manip.getSize(), 3u);
            }
        }
    }
    TEST_F(DrawVulkanIntegrationTest, SetterValidatedCommandCanBeReusedForMultipleDraws) {
        vsl::command::Draw draw;
        draw.setSize(3);
        expect_triangle_color(render([&](auto& stream) { stream << draw << draw; }));
        EXPECT_EQ(draw.getSize(), 3u);
    }
} // namespace
