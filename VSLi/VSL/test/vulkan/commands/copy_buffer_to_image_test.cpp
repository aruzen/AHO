#include "support/image_transfer_fixture.hpp"
namespace {
    using CopyBufferToImageVulkanIntegrationTest = vsl_test::ImageTransferVulkanIntegrationTest;
    TEST_F(CopyBufferToImageVulkanIntegrationTest, CopiesEveryTexelAndSelectedArrayLayer) {
        const std::array<std::uint32_t, 4> expected{0x12345678, 0x90abcdef, 0, 0xffffffff};
        EXPECT_EQ(round_trip(false, false), expected);
        EXPECT_EQ(round_trip(false, true), expected);
    }
    TEST_F(CopyBufferToImageVulkanIntegrationTest, TypedConstructorRetainsImageBufferAndDeclaredLayout) {
        TransferImage image(device, 2, 2, vsl::data_format::UNSIGNED_INT);
        vsl::StagingBuffer<vsl::MemoryType::TRANSFER_SOURCE> source(device, std::size_t{16});
        vsl::command::CopyBufferToImage command(image, source);
        EXPECT_EQ(command.image.data_, image.data_);
        EXPECT_EQ(command.buffer, &source);
        EXPECT_EQ(command.layout, vsl::ImageLayout::UNDEFINED);
        // UNDEFINED is not a valid transfer layout; invocation of this specialization is excluded.
    }
} // namespace

namespace {
    TEST_F(CopyBufferToImageVulkanIntegrationTest, TypedArrayCopyDelegatesToItsOwnConstructor) {
        const std::array<std::uint32_t, 4> expected{5, 13, 29, 0xffffffffu};
        TransferImage image(device, 2, 2, vsl::data_format::UNSIGNED_INT, 2);
        vsl::StagingBuffer<vsl::MemoryType::TRANSFER_SOURCE> source(device, expected);
        vsl::StagingBuffer<vsl::MemoryType::TRANSFER_DESTINATION> readback(device, sizeof(expected));
        vsl::command::CopyBufferToArrayImage command(image, source, 1);
        EXPECT_EQ(command.image.data_, image.data_);
        EXPECT_EQ(command.buffer, &source);
        EXPECT_EQ(command.layout, vsl::ImageLayout::UNDEFINED);
        EXPECT_EQ(command.target_idx, 1u);
        auto manager = make_manager();
        {
            auto phase = manager.start_phase<vsl::ComputePhase>();
            transition(manager, image, vk::ImageLayout::eUndefined, vk::ImageLayout::eGeneral, {},
                       vk::AccessFlagBits::eTransferWrite);
            // The template records the initial layout; use the transitioned layout for the command.
            command.layout = vsl::ImageLayout::GENERAL;
            phase << command;
            transition(manager, image, vk::ImageLayout::eGeneral, vk::ImageLayout::eGeneral,
                       vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eTransferRead);
            vk::BufferImageCopy region{};
            region.imageSubresource = {vk::ImageAspectFlagBits::eColor, 0, 1, 1};
            region.imageExtent = {2, 2, 1};
            auto buffer = manager.get_buffer();
            buffer.data_->command_buffers[buffer.get_current_buffer_idx()].copyImageToBuffer(
                image.data_->image, vk::ImageLayout::eGeneral, readback.data_->buffer, region);
            vk::MemoryBarrier host_visibility{};
            host_visibility.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
            host_visibility.dstAccessMask = vk::AccessFlagBits::eHostRead;
            auto commands = manager.get_buffer();
            commands.data_->command_buffers[commands.get_current_buffer_idx()].pipelineBarrier(
                vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eHost, {}, host_visibility, {}, {});
        }
        device.data_->device.waitIdle();
        auto mapped = readback.data(sizeof(expected));
        EXPECT_EQ(std::memcmp(mapped.data, expected.data(), sizeof(expected)), 0);
    }

    TEST_F(CopyBufferToImageVulkanIntegrationTest, CommandsCaptureStorageBeforeSourceHandleIsDestroyed) {
        const std::array<std::uint32_t, 4> expected{17, 29, 43, 71};
        TransferImage image(device, 2, 2, vsl::data_format::UNSIGNED_INT, 2);
        std::optional<vsl::command::CopyBufferToImage> single;
        std::optional<vsl::command::CopyBufferToArrayImage> array;
        std::weak_ptr<vsl::impl::BufferImpl> source_storage;
        {
            vsl::StagingBuffer<> source(device, expected);
            source_storage = source.data_;
            single.emplace(image, &source, vsl::ImageLayout::GENERAL);
            array.emplace(image, &source, vsl::ImageLayout::GENERAL, 1);
        }
        ASSERT_FALSE(source_storage.expired());
        auto manager = make_manager();
        vsl::StagingBuffer<vsl::MemoryType::TRANSFER_DESTINATION> first(device, sizeof(expected)), second(device, sizeof(expected));
        {
            vsl::ComputePhase phase(manager);
            transition(manager, image, vk::ImageLayout::eUndefined, vk::ImageLayout::eGeneral, {}, vk::AccessFlagBits::eTransferWrite);
            phase << *single << *array;
            single.reset();
            array.reset();
            transition(manager, image, vk::ImageLayout::eGeneral, vk::ImageLayout::eGeneral,
                       vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eTransferRead);
            phase << vsl::command::CopyImageToBuffer(&first, image, vsl::ImageLayout::GENERAL, {0, 0, 2, 2, 0}, true)
                  << vsl::command::CopyImageToBuffer(&second, image, vsl::ImageLayout::GENERAL, {0, 0, 2, 2, 1}, true);
            ASSERT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
            ASSERT_TRUE(phase.completion().wait());
        }
        auto first_mapping = first.data();
        auto second_mapping = second.data();
        EXPECT_EQ(std::memcmp(first_mapping.read(sizeof(expected)).data(), expected.data(), sizeof(expected)), 0);
        EXPECT_EQ(std::memcmp(second_mapping.read(sizeof(expected)).data(), expected.data(), sizeof(expected)), 0);
    }

} // namespace
