#include "support/render_fixture.hpp"
namespace {
    class DrawIndexedVulkanIntegrationTest : public vsl_test::RenderVulkanIntegrationTest {
        bool validate_vulkan() const override { return true; }
    };
    TEST(DrawIndexedTest, ConstructorsAndManipulatorRetainIndexCount) {
        EXPECT_EQ(vsl::command::DrawIndexed{}.size, 0u);
        EXPECT_EQ(vsl::command::DrawIndexed{3}.size, 3u);
        vsl::command::DrawIndexedManip draw;
        draw.setVertexSize(3);
        EXPECT_EQ(draw.size, 3u);
        draw.setVertexSize(std::nullopt);
        EXPECT_EQ(draw.size, 3u);
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
