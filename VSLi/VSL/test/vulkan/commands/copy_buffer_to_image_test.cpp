#include "support/image_transfer_fixture.hpp"
namespace {
    using CopyBufferToImageVulkanIntegrationTest = vsl_test::ImageTransferVulkanIntegrationTest;
    TEST_F(CopyBufferToImageVulkanIntegrationTest, CopiesEveryTexelAndSelectedArrayLayer) {
        const std::array<std::uint32_t, 4> expected{0x12345678, 0x90abcdef, 0, 0xffffffff};
        EXPECT_EQ(round_trip(false, false), expected);
        EXPECT_EQ(round_trip(false, true), expected);
    }
    TEST_F(CopyBufferToImageVulkanIntegrationTest, TypedConstructorRetainsImageBufferAndDeclaredLayout) {
        TransferImage image(device, 2, 2, vsl::data_format::UnsignedInt);
        vsl::StagingBuffer<vsl::MemoryType::TransferSource> source(device, std::size_t{16});
        vsl::command::CopyBufferToImage command(image, source);
        EXPECT_EQ(command.image._data, image._data);
        EXPECT_EQ(command.buffer, &source);
        EXPECT_EQ(command.layout, vsl::ImageLayout::Undefined);
        // Undefined is not a valid transfer layout; invocation of this specialization is excluded.
    }
} // namespace

namespace {
    TEST_F(CopyBufferToImageVulkanIntegrationTest, TypedArrayCopyDelegatesToItsOwnConstructor) {
        const std::array<std::uint32_t, 4> expected{5, 13, 29, 0xffffffffu};
        TransferImage image(device, 2, 2, vsl::data_format::UnsignedInt, 2);
        vsl::StagingBuffer<vsl::MemoryType::TransferSource> source(device, expected);
        vsl::StagingBuffer<vsl::MemoryType::TransferDestination> readback(device, sizeof(expected));
        vsl::command::CopyBufferToArrayImage command(image, source, 1);
        EXPECT_EQ(command.image._data, image._data);
        EXPECT_EQ(command.buffer, &source);
        EXPECT_EQ(command.layout, vsl::ImageLayout::Undefined);
        EXPECT_EQ(command.target_idx, 1u);
        auto manager = make_manager();
        {
            auto phase = manager.startPhase<vsl::ComputePhase>();
            transition(manager, image, vk::ImageLayout::eUndefined, vk::ImageLayout::eGeneral, {},
                       vk::AccessFlagBits::eTransferWrite);
            // The template records the initial layout; use the transitioned layout for the command.
            command.layout = vsl::ImageLayout::General;
            phase << command;
            transition(manager, image, vk::ImageLayout::eGeneral, vk::ImageLayout::eGeneral,
                       vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eTransferRead);
            vk::BufferImageCopy region{};
            region.imageSubresource = {vk::ImageAspectFlagBits::eColor, 0, 1, 1};
            region.imageExtent = {2, 2, 1};
            auto buffer = manager.getBuffer();
            buffer._data->commandBuffers[buffer.getCurrentBufferIdx()].copyImageToBuffer(
                image._data->image, vk::ImageLayout::eGeneral, readback._data->buffer, region);
            vk::MemoryBarrier host_visibility{};
            host_visibility.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
            host_visibility.dstAccessMask = vk::AccessFlagBits::eHostRead;
            auto commands = manager.getBuffer();
            commands._data->commandBuffers[commands.getCurrentBufferIdx()].pipelineBarrier(
                vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eHost, {}, host_visibility, {}, {});
        }
        device._data->device.waitIdle();
        auto mapped = readback.data(sizeof(expected));
        EXPECT_EQ(std::memcmp(mapped.data, expected.data(), sizeof(expected)), 0);
    }

} // namespace
