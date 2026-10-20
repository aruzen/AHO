#include "support/vulkan_fixture.hpp"
#include <VSL/vulkan/commands/copy_buffer.hpp>

namespace {
    class CopyBufferVulkanIntegrationTest : public vsl_test::VulkanIntegrationTest {
        bool validate_vulkan() const override { return true; }
    };
    using Native = vsl::Buffer<vsl::MemoryType::TransferSource | vsl::MemoryType::TransferDestination, vsl::MemoryProperty::HostVisible>;
    TEST_F(CopyBufferVulkanIntegrationTest, UnalignedRangesAndDeferredCommandsRetainTheOriginalBuffers) {
        auto manager = make_manager();
        Native destination(device, 8);
        auto mapping = destination.data();
        mapping.write(std::array<std::byte, 8>{});
        std::optional<vsl::command::CopyBuffer> copy;
        std::weak_ptr<vsl::_impl::Buffer_impl> sourceWeak;
        {
            Native source(device, 8);
            sourceWeak = source._data;
            source.data().write(std::array<std::byte, 8>{std::byte{11}, std::byte{22}, std::byte{33}, std::byte{44}, std::byte{55}, std::byte{66}, std::byte{77}, std::byte{88}});
            copy.emplace(&source, &destination, 3, 1, 2, true);
        }
        ASSERT_FALSE(sourceWeak.expired());
        auto phase = manager.startPhase<vsl::ComputePhase>();
        copy->validate();
        phase << *copy;
        copy.reset();
        ASSERT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::Success);
        ASSERT_TRUE(phase.completion().wait());
        const auto bytes = mapping.read(8);
        EXPECT_EQ(bytes[1], std::byte{});
        EXPECT_EQ(bytes[2], std::byte{22});
        EXPECT_EQ(bytes[4], std::byte{44});
        EXPECT_EQ(bytes[5], std::byte{});
    }
    TEST_F(CopyBufferVulkanIntegrationTest, InvalidContractsAreRejectedBeforeRecording) {
        Native a(device, 8), b(device, 8);
        vsl::Buffer<vsl::MemoryType::UniformBuffer, vsl::MemoryProperty::HostVisible> wrong(device, 8);
        EXPECT_THROW((vsl::command::CopyBuffer(&a, &b, 0).validate()), std::out_of_range);
        EXPECT_THROW((vsl::command::CopyBuffer(&a, &b, 1, SIZE_MAX).validate()), std::out_of_range);
        EXPECT_THROW((vsl::command::CopyBuffer(&a, &b, 4, 0, 6).validate()), std::out_of_range);
        EXPECT_THROW((vsl::command::CopyBuffer(&a, &a, 4, 0, 2).validate()), std::invalid_argument);
        EXPECT_THROW((vsl::command::CopyBuffer(&wrong, &b, 4).validate()), std::invalid_argument);
        EXPECT_THROW((vsl::command::CopyBuffer(&a, &wrong, 4).validate()), std::invalid_argument);
        EXPECT_THROW((vsl::command::CopyBuffer(nullptr, &b, 4).validate()), std::invalid_argument);
        auto manager = make_manager();
        vsl::CommandBuffer empty(std::shared_ptr<vsl::_impl::CommandBuffer_impl>{});
        EXPECT_THROW(vsl::command::CopyBuffer(&a, &b, 4).invoke(manager.getPool(), empty, manager), std::invalid_argument);
    }
} // namespace
