#include <AHO/x/resource/buffer.hpp>
#include <gtest/gtest.h>
#include <array>
#include <utility>

#ifndef AHO_GPU_TESTS
namespace {
    namespace resource = aho::x::resource;
    TEST(ExperimentalBufferTest, ReserveResizeAndAliasesShareStorageWithoutGrowingWithinCapacity) {
        resource::Buffer buffer(4);
        EXPECT_EQ(buffer.size(), 4u);
        buffer.reserve(32);
        auto* storage = buffer.data().data();
        auto alias = buffer;
        buffer.resize(8);
        EXPECT_EQ(buffer.data().data(), storage);
        EXPECT_EQ(alias.size(), 8u);
        EXPECT_GE(alias.capacity(), 32u);
        const std::array<std::byte, 2> bytes{std::byte{17}, std::byte{29}};
        alias.write(bytes, 2);
        EXPECT_EQ(buffer.data()[2], bytes[0]);
        const auto& view = buffer;
        EXPECT_EQ(view.data()[3], bytes[1]);
        EXPECT_EQ(view.data()[7], std::byte{});
        auto moved = std::move(buffer);
        EXPECT_EQ(buffer.size(), 0u);
        EXPECT_TRUE(buffer.data().empty());
        EXPECT_EQ(moved.data().data(), storage);
    }
    TEST(ExperimentalBufferTest, BoundsAndOverlappingWritesPreserveThePayload) {
        resource::Buffer buffer(4);
        const std::array<std::byte, 4> initial{std::byte{1}, std::byte{2}, std::byte{3}, std::byte{4}};
        buffer.write(initial);
        buffer.write(buffer.data().first(3), 1);
        EXPECT_EQ(buffer.data()[0], std::byte{1});
        EXPECT_EQ(buffer.data()[1], std::byte{1});
        EXPECT_EQ(buffer.data()[2], std::byte{2});
        EXPECT_EQ(buffer.data()[3], std::byte{3});
        EXPECT_THROW(buffer.write(initial, 1), std::out_of_range);
        EXPECT_THROW(buffer.write({}, SIZE_MAX), std::out_of_range);
        EXPECT_NO_THROW(buffer.write({}, buffer.size()));
        buffer.resize(0);
        EXPECT_TRUE(buffer.data().empty());
        EXPECT_GE(buffer.capacity(), 4u);
    }
} // namespace
#else
#include "support/vulkan_fixture.hpp"
namespace {
    class UploadBufferVulkanIntegrationTest : public vsl_test::VulkanIntegrationTest {
        bool validate_vulkan() const override { return true; }
    };
    TEST_F(UploadBufferVulkanIntegrationTest, LogicalLengthAndReservedCapacityAreIndependent) {
        aho::x::resource::UploadBuffer buffer(device);
        EXPECT_EQ(buffer.size(), 0u);
        EXPECT_EQ(buffer.capacity(), 0u);
        EXPECT_FALSE(buffer.busy());
        EXPECT_TRUE(buffer.wait(0));
        buffer.reserve(64);
        const std::array<std::byte, 8> payload{};
        buffer.assign(payload);
        EXPECT_EQ(buffer.size(), 8u);
        EXPECT_EQ(buffer.capacity(), 64u);
        auto alias = buffer;
        alias.assign(std::span(payload).first(4));
        EXPECT_EQ(buffer.size(), 4u);
        EXPECT_EQ(buffer.capacity(), 64u);
        buffer.reserve(128);
        EXPECT_EQ(alias.capacity(), 128u);
        EXPECT_EQ(alias.size(), 4u);
        buffer.write({});
    }
    TEST_F(UploadBufferVulkanIntegrationTest, InvalidWritesDoNotChangeSizeAndEmptyAssignmentKeepsCapacity) {
        aho::x::resource::UploadBuffer buffer(device, 16);
        std::array<std::byte, 4> payload{};
        buffer.assign(payload);
        EXPECT_THROW(buffer.write(payload, 1), std::out_of_range);
        EXPECT_THROW(buffer.write({}, SIZE_MAX), std::out_of_range);
        EXPECT_NO_THROW(buffer.write({}, 4));
        EXPECT_EQ(buffer.size(), 4u);
        buffer.assign({});
        EXPECT_EQ(buffer.size(), 0u);
        EXPECT_EQ(buffer.capacity(), 16u);
        EXPECT_THROW((aho::x::resource::UploadBuffer(vsl::LogicalDeviceAccessor{})), std::invalid_argument);
    }
} // namespace
#endif
