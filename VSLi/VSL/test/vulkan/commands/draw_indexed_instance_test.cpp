#include "support/render_fixture.hpp"
namespace {
    class DrawInstanceVulkanIntegrationTest : public vsl_test::RenderVulkanIntegrationTest {
        bool validate_vulkan() const override { return true; }
    };
    TEST(DrawInstanceTest, ConstructorsRetainBothCounts) {
        const vsl::command::DrawInstance explicit_counts(3, 2), implicit_count(4);
        EXPECT_EQ(explicit_counts.index_size, 3u);
        EXPECT_EQ(explicit_counts.instance_size, 2u);
        EXPECT_EQ(implicit_count.index_size, 0u);
        EXPECT_EQ(implicit_count.instance_size, 4u);
    }
    TEST_F(DrawInstanceVulkanIntegrationTest, DrawsTwoIndexedInstances) {
        std::array<std::uint32_t, 3> indices{0, 1, 2};
        vsl::StagingBuffer<vsl::MemoryType::IndexBuffer> index(device, indices);
        expect_triangle_color(render([&](auto& stream) {
            stream << vsl::command::BindIndexBuffer(index) << vsl::command::DrawInstance{3, 2};
        }));
    }
} // namespace

#include <limits>
namespace {
    TEST_F(DrawInstanceVulkanIntegrationTest, OversizedIndexAndInstanceCountsAreRejectedBeforeRecording) {
        if constexpr (sizeof(std::size_t) <= sizeof(std::uint32_t)) {
            GTEST_SKIP() << "size_t has no values above uint32_t";
        } else {
            const auto overflow = std::size_t{std::numeric_limits<std::uint32_t>::max()} + 1;
            std::array<std::uint32_t, 3> indices{0, 1, 2};
            vsl::StagingBuffer<vsl::MemoryType::IndexBuffer> index(device, indices);
            expect_triangle_color(render([&](auto& stream) {
                stream << vsl::command::BindIndexBuffer(index);
                EXPECT_THROW(stream << vsl::command::DrawInstance(overflow, 1), vsl::exceptions::RuntimeException);
                EXPECT_THROW(stream << vsl::command::DrawInstance(3, overflow), vsl::exceptions::RuntimeException);
                EXPECT_THROW(stream << vsl::command::DrawInstance(overflow), vsl::exceptions::RuntimeException);
                stream << vsl::command::DrawInstance(3, 2);
            }));
        }
    }
} // namespace
