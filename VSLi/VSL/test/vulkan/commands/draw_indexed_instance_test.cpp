#include "support/render_fixture.hpp"
namespace {
    class DrawInstanceVulkanIntegrationTest : public vsl_test::RenderVulkanIntegrationTest {
        bool validate_vulkan() const override { return true; }
    };
    TEST(DrawInstanceTest, ConstructorsRetainBothCounts) {
        const vsl::command::DrawInstance explicit_counts(3, 2), implicit_count(4);
        EXPECT_EQ(explicit_counts.getIndexSize(), 3u);
        EXPECT_EQ(explicit_counts.getInstanceSize(), 2u);
        EXPECT_EQ(implicit_count.getIndexSize(), 0u);
        EXPECT_EQ(implicit_count.getInstanceSize(), 4u);
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

namespace {
    TEST(DrawInstanceTest, ConstructorsAndSettersKeepTheFullNativeCountRange) {
        for (const auto count : {std::size_t{0}, std::size_t{1}, std::size_t{std::numeric_limits<std::uint32_t>::max()}}) {
            vsl::command::DrawInstance draw(count, count), instances_only(count);
            EXPECT_EQ(draw.getIndexSize(), count);
            EXPECT_EQ(draw.getInstanceSize(), count);
            EXPECT_EQ(instances_only.getIndexSize(), 0u);
            EXPECT_EQ(instances_only.getInstanceSize(), count);
            draw.setIndexSize(7);
            draw.setInstanceSize(11);
            draw.setIndexSize(count);
            draw.setInstanceSize(count);
            EXPECT_EQ(draw.getIndexSize(), count);
            EXPECT_EQ(draw.getInstanceSize(), count);
        }
    }
    TEST(DrawInstanceTest, InvalidInputsAreRejectedBeforeRecordingAndKeepThePreviousCounts) {
        if constexpr (sizeof(std::size_t) <= sizeof(std::uint32_t)) {
            GTEST_SKIP() << "size_t has no values above uint32_t";
        } else {
            vsl::command::DrawInstance draw(3, 2);
            const auto overflow = std::size_t{std::numeric_limits<std::uint32_t>::max()} + 1;
            for (const auto count : {overflow, std::numeric_limits<std::size_t>::max()}) {
                EXPECT_THROW((vsl::command::DrawInstance(count, 1)), vsl::exceptions::RuntimeException);
                EXPECT_THROW((vsl::command::DrawInstance(1, count)), vsl::exceptions::RuntimeException);
                EXPECT_THROW((vsl::command::DrawInstance(count)), vsl::exceptions::RuntimeException);
                EXPECT_THROW(draw.setIndexSize(count), vsl::exceptions::RuntimeException);
                EXPECT_EQ(draw.getIndexSize(), 3u);
                EXPECT_EQ(draw.getInstanceSize(), 2u);
                EXPECT_THROW(draw.setInstanceSize(count), vsl::exceptions::RuntimeException);
                EXPECT_EQ(draw.getIndexSize(), 3u);
                EXPECT_EQ(draw.getInstanceSize(), 2u);
            }
        }
    }
    TEST_F(DrawInstanceVulkanIntegrationTest, SetterValidatedCommandDrawsTheRequestedInstances) {
        vsl::command::DrawInstance draw(0, 0);
        draw.setIndexSize(3);
        draw.setInstanceSize(2);
        std::array<std::uint32_t, 3> indices{0, 1, 2};
        vsl::StagingBuffer<vsl::MemoryType::IndexBuffer> index(device, indices);
        expect_triangle_color(render([&](auto& stream) { stream << vsl::command::BindIndexBuffer(index) << draw; }));
        EXPECT_EQ(draw.getIndexSize(), 3u);
        EXPECT_EQ(draw.getInstanceSize(), 2u);
    }
} // namespace
