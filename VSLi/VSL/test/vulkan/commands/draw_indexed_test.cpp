#include "support/render_fixture.hpp"
namespace {
    class DrawIndexedVulkanIntegrationTest : public vsl_test::RenderVulkanIntegrationTest {
        bool validate_vulkan() const override { return true; }
    };
    TEST(DrawIndexedTest, ConstructorsAndManipulatorRetainIndexCount) {
        EXPECT_EQ(vsl::command::DrawIndexed{}.getSize(), 0u);
        EXPECT_EQ(vsl::command::DrawIndexed{3}.getSize(), 3u);
        vsl::command::DrawIndexedManip draw;
        draw.setVertexSize(3);
        EXPECT_EQ(draw.getSize(), 3u);
        draw.setVertexSize(std::nullopt);
        EXPECT_EQ(draw.getSize(), 3u);
    }
    TEST_F(DrawIndexedVulkanIntegrationTest, DrawsIndexedTriangle) {
        std::array<std::uint32_t, 3> indices{0, 1, 2};
        vsl::StagingBuffer<vsl::MemoryType::IndexBuffer> index(device, indices);
        expect_triangle_color(render([&](auto& stream) {
            stream << vsl::command::BindIndexBuffer(index) << vsl::command::DrawIndexed{3};
        }));
    }
} // namespace

#include <limits>
namespace {
    TEST_F(DrawIndexedVulkanIntegrationTest, OversizedIndexedAndManipulatorCountsAreRejectedBeforeRecording) {
        if constexpr (sizeof(std::size_t) <= sizeof(std::uint32_t)) {
            GTEST_SKIP() << "size_t has no values above uint32_t";
        } else {
            const auto overflow = std::size_t{std::numeric_limits<std::uint32_t>::max()} + 1;
            std::array<std::uint32_t, 3> indices{0, 1, 2};
            vsl::StagingBuffer<vsl::MemoryType::IndexBuffer> index(device, indices);
            expect_triangle_color(render([&](auto& stream) {
                stream << vsl::command::BindIndexBuffer(index);
                EXPECT_THROW(stream << vsl::command::DrawIndexed(overflow), vsl::exceptions::RuntimeException);
                stream.vertexSize = overflow;
                EXPECT_THROW(stream << vsl::command::drawIndexed, vsl::exceptions::RuntimeException);
                stream.vertexSize = 3;
                stream << vsl::command::drawIndexed;
            }));
        }
    }
} // namespace

namespace {
    TEST(DrawIndexedTest, ConstructorsAndSettersKeepTheFullNativeCountRange) {
        for (const auto count : {std::size_t{0}, std::size_t{1}, std::size_t{std::numeric_limits<std::uint32_t>::max()}}) {
            vsl::command::DrawIndexed draw(count);
            EXPECT_EQ(draw.getSize(), count);
            draw.setSize(7);
            draw.setSize(count);
            EXPECT_EQ(draw.getSize(), count);
            vsl::command::DrawIndexedManip manip;
            manip.setVertexSize(count);
            EXPECT_EQ(manip.getSize(), count);
            manip.setVertexSize(std::nullopt);
            EXPECT_EQ(manip.getSize(), count);
        }
    }
    TEST(DrawIndexedTest, InvalidInputsAreRejectedBeforeRecordingAndKeepThePreviousCount) {
        if constexpr (sizeof(std::size_t) <= sizeof(std::uint32_t)) {
            GTEST_SKIP() << "size_t has no values above uint32_t";
        } else {
            vsl::command::DrawIndexed draw(3);
            vsl::command::DrawIndexedManip manip;
            manip.setVertexSize(3);
            const auto overflow = std::size_t{std::numeric_limits<std::uint32_t>::max()} + 1;
            for (const auto count : {overflow, std::numeric_limits<std::size_t>::max()}) {
                EXPECT_THROW((vsl::command::DrawIndexed(count)), vsl::exceptions::RuntimeException);
                EXPECT_THROW(draw.setSize(count), vsl::exceptions::RuntimeException);
                EXPECT_EQ(draw.getSize(), 3u);
                EXPECT_THROW(manip.setSize(count), vsl::exceptions::RuntimeException);
                EXPECT_THROW(manip.setVertexSize(count), vsl::exceptions::RuntimeException);
                EXPECT_EQ(manip.getSize(), 3u);
            }
        }
    }
    TEST_F(DrawIndexedVulkanIntegrationTest, SetterValidatedCommandCanBeReusedForMultipleDraws) {
        vsl::command::DrawIndexed draw;
        draw.setSize(3);
        std::array<std::uint32_t, 3> indices{0, 1, 2};
        vsl::StagingBuffer<vsl::MemoryType::IndexBuffer> index(device, indices);
        expect_triangle_color(render([&](auto& stream) { stream << vsl::command::BindIndexBuffer(index) << draw << draw; }));
        EXPECT_EQ(draw.getSize(), 3u);
    }
} // namespace
