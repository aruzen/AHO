#include "support/image_transfer_fixture.hpp"
#include <VSL/vulkan/commands/image_readback.hpp>
#include <VSL/vulkan/commands/change_image_barrier.hpp>

namespace {
    class ImageReadbackCommandVulkanIntegrationTest : public vsl_test::ImageTransferVulkanIntegrationTest {
        bool validate_vulkan() const override;

    protected:
        TransferImage initialized_image(vsl::CommandManager manager);
    };

    bool ImageReadbackCommandVulkanIntegrationTest::validate_vulkan() const {
        return true;
    }

    ImageReadbackCommandVulkanIntegrationTest::TransferImage ImageReadbackCommandVulkanIntegrationTest::initialized_image(vsl::CommandManager manager) {
        const std::array<std::uint32_t, 4> values{11, 22, 33, 44};
        TransferImage image(device, 2, 2, vsl::data_format::UNSIGNED_INT);
        vsl::StagingBuffer<> source(device, values);
        vsl::ComputePhase phase(manager);
        phase << vsl::command::ChangeImageBarrier(image, vsl::ImageLayout::TRANSFER_DST_OPTIMAL)
              << vsl::command::CopyBufferToImage(image, &source, vsl::ImageLayout::TRANSFER_DST_OPTIMAL);
        if (phase.submit() != vsl::ComputePhase::SubmitResult::SUCCESS || !phase.completion().wait())
            throw std::runtime_error("Image setup submission failed");
        return image;
    }

    TEST_F(ImageReadbackCommandVulkanIntegrationTest, RecordsCopyAndRestoresLayoutWithoutSubmitting) {
        const std::array<std::uint32_t, 4> first{11, 22, 33, 44}, second{55, 66, 77, 88};
        TransferImage image(device, 2, 2, vsl::data_format::UNSIGNED_INT);
        vsl::StagingBuffer<> first_source(device, first), second_source(device, second);
        vsl::StagingBuffer<vsl::MemoryType::TRANSFER_DESTINATION> pixel(device, sizeof(std::uint32_t));
        vsl::StagingBuffer<vsl::MemoryType::TRANSFER_DESTINATION> full(device, sizeof(second));
        auto manager = make_manager();
        {
            auto phase = manager.start_phase<vsl::ComputePhase>();
            phase << vsl::command::ChangeImageBarrier(image, vsl::ImageLayout::TRANSFER_DST_OPTIMAL)
                  << vsl::command::CopyBufferToImage(image, &first_source, vsl::ImageLayout::TRANSFER_DST_OPTIMAL)
                  << vsl::command::ImageReadback(&pixel, image, vsl::ImageLayout::TRANSFER_DST_OPTIMAL, {1, 0, 1, 1})
                  << vsl::command::CopyBufferToImage(image, &second_source, vsl::ImageLayout::TRANSFER_DST_OPTIMAL)
                  << vsl::command::ImageReadback(full.data_, image, vsl::ImageLayout::TRANSFER_DST_OPTIMAL);
            EXPECT_EQ(phase.state, vsl::ComputePhase::State::WAIT_SUBMIT);
        }
        device.data_->device.waitIdle();
        auto mapped_pixel = pixel.data(sizeof(std::uint32_t));
        std::uint32_t actual_pixel;
        std::memcpy(&actual_pixel, mapped_pixel.data, sizeof(actual_pixel));
        EXPECT_EQ(actual_pixel, first[1]);
        auto mapped_full = full.data(sizeof(second));
        std::array<std::uint32_t, 4> actual;
        std::memcpy(actual.data(), mapped_full.data, sizeof(actual));
        EXPECT_EQ(actual, second);
    }

    TEST_F(ImageReadbackCommandVulkanIntegrationTest, SelectedLayerRestorationLeavesOtherLayersUsable) {
        const std::array<std::uint32_t, 4> expected{11, 22, 33, 44};
        TransferImage image(device, 2, 2, vsl::data_format::UNSIGNED_INT, 2);
        vsl::StagingBuffer<> source(device, expected);
        vsl::StagingBuffer<vsl::MemoryType::TRANSFER_DESTINATION> pixel(device, sizeof(std::uint32_t));
        vsl::StagingBuffer<vsl::MemoryType::TRANSFER_DESTINATION> full(device, sizeof(expected));
        auto manager = make_manager();
        {
            auto phase = manager.start_phase<vsl::ComputePhase>();
            phase << vsl::command::ChangeImageBarrier(image, vsl::ImageLayout::TRANSFER_DST_OPTIMAL)
                  << vsl::command::CopyBufferToArrayImage(image, &source, vsl::ImageLayout::TRANSFER_DST_OPTIMAL, 0)
                  << vsl::command::CopyBufferToArrayImage(image, &source, vsl::ImageLayout::TRANSFER_DST_OPTIMAL, 1)
                  << vsl::command::ImageReadback(&pixel, image, vsl::ImageLayout::TRANSFER_DST_OPTIMAL, {1, 1, 1, 1, 1})
                  << vsl::command::CopyBufferToArrayImage(image, &source, vsl::ImageLayout::TRANSFER_DST_OPTIMAL, 1)
                  << vsl::command::ImageReadback(&full, image, vsl::ImageLayout::TRANSFER_DST_OPTIMAL, {0, 0, 2, 2, 0});
        }
        device.data_->device.waitIdle();
        auto mapped = pixel.data(sizeof(std::uint32_t));
        std::uint32_t actual;
        std::memcpy(&actual, mapped.data, sizeof(actual));
        EXPECT_EQ(actual, expected[3]);
    }

    TEST_F(ImageReadbackCommandVulkanIntegrationTest, DescriptionAndInvalidContractsDoNotRecordCommands) {
        TransferImage image(device, 2, 2, vsl::data_format::UNSIGNED_INT);
        const auto info = vsl::command::ImageReadback::Describe(image, vsl::ImageLayout::GENERAL);
        EXPECT_EQ(info.region.width, 2u);
        EXPECT_EQ(info.region.height, 2u);
        EXPECT_EQ(info.row_stride, 8u);
        EXPECT_EQ(info.byte_size, 16u);
        EXPECT_EQ(info.format, vsl::data_format::UNSIGNED_INT);
        vsl::StagingBuffer<vsl::MemoryType::TRANSFER_DESTINATION> small(device, 4);
        auto manager = make_manager();
        auto invoke = [&](vsl::ImageLayout layout, vsl::ImageRegion region = {}) {
            vsl::command::ImageReadback(&small, image, layout, region)
                .invoke(manager.get_pool(), manager.get_buffer(), manager);
        };
        EXPECT_THROW(invoke(vsl::ImageLayout::UNDEFINED), std::invalid_argument);
        EXPECT_THROW(invoke(vsl::ImageLayout::PREINITIALIZED), std::invalid_argument);
        EXPECT_THROW(invoke(vsl::ImageLayout::GENERAL), std::out_of_range);
        EXPECT_THROW(invoke(vsl::ImageLayout::GENERAL, {2, 0, 1, 1}), std::out_of_range);
    }
} // namespace

namespace {
    TEST_F(ImageReadbackCommandVulkanIntegrationTest, EmptyCommandBufferIsRejectedBeforeItIsDereferenced) {
        auto manager = make_manager();
        auto image = initialized_image(manager);
        vsl::StagingBuffer<vsl::MemoryType::TRANSFER_DESTINATION> destination(device, std::size_t{16});
        const vsl::CommandBuffer empty(std::shared_ptr<vsl::impl::CommandBufferImpl>{});
        vsl::command::ImageReadback command(&destination, image, vsl::ImageLayout::TRANSFER_DST_OPTIMAL);
        EXPECT_THROW(command.invoke(manager.get_pool(), empty, manager), std::invalid_argument);
    }
    TEST_F(ImageReadbackCommandVulkanIntegrationTest, EmptyManagerIsRejectedBeforeRetainingResourcesOrRecordingBarriers) {
        auto manager = make_manager();
        auto image = initialized_image(manager);
        vsl::StagingBuffer<vsl::MemoryType::TRANSFER_DESTINATION> destination(device, std::size_t{16});
        const vsl::CommandManager empty(std::shared_ptr<vsl::impl::CommandManagerImpl>{});
        vsl::ComputePhase phase(manager);
        const auto buffer = manager.get_buffer();
        ASSERT_TRUE(buffer.data_->recording_submission);
        const auto retained = buffer.data_->recording_submission->resources;
        vsl::command::ImageReadback command(&destination, image, vsl::ImageLayout::TRANSFER_DST_OPTIMAL);
        EXPECT_THROW(command.invoke(manager.get_pool(), buffer, empty), std::invalid_argument);
        EXPECT_EQ(buffer.data_->recording_submission->resources, retained);
        phase.cancel();
    }
    TEST_F(ImageReadbackCommandVulkanIntegrationTest, CommandKeepsDestinationWhenOriginalHandleIsDestroyedBeforeRecording) {
        auto manager = make_manager();
        auto image = initialized_image(manager);
        std::optional<vsl::command::ImageReadback> command;
        std::optional<vsl::BufferAccessor::LocalBufferHolder> mapped;
        std::weak_ptr<vsl::impl::BufferImpl> storage;
        {
            vsl::Buffer<vsl::MemoryType::TRANSFER_DESTINATION, vsl::MemoryProperty::HOST_VISIBLE> destination(device, std::size_t{16});
            storage = destination.data_;
            mapped.emplace(destination.data());
            command.emplace(&destination, image, vsl::ImageLayout::TRANSFER_DST_OPTIMAL);
        }
        ASSERT_FALSE(storage.expired());
        {
            vsl::ComputePhase phase(manager);
            phase << *command;
            command.reset();
            ASSERT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
            ASSERT_TRUE(phase.completion().wait());
        }
        std::array<std::uint32_t, 4> pixels{};
        auto bytes = mapped->read(sizeof(pixels));
        std::memcpy(pixels.data(), bytes.data(), bytes.size());
        EXPECT_EQ(pixels, (std::array<std::uint32_t, 4>{11, 22, 33, 44}));
    }

} // namespace
