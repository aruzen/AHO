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
        TransferImage image(device, 2, 2, vsl::data_format::UnsignedInt);
        vsl::StagingBuffer<> source(device, values);
        vsl::ComputePhase phase(manager);
        phase << vsl::command::ChangeImageBarrier(image, vsl::ImageLayout::TransferDstOptimal)
              << vsl::command::CopyBufferToImage(image, &source, vsl::ImageLayout::TransferDstOptimal);
        if (phase.submit() != vsl::ComputePhase::SubmitResult::Success || !phase.completion().wait())
            throw std::runtime_error("Image setup submission failed");
        return image;
    }

    TEST_F(ImageReadbackCommandVulkanIntegrationTest, RecordsCopyAndRestoresLayoutWithoutSubmitting) {
        const std::array<std::uint32_t, 4> first{11, 22, 33, 44}, second{55, 66, 77, 88};
        TransferImage image(device, 2, 2, vsl::data_format::UnsignedInt);
        vsl::StagingBuffer<> firstSource(device, first), secondSource(device, second);
        vsl::StagingBuffer<vsl::MemoryType::TransferDestination> pixel(device, sizeof(std::uint32_t));
        vsl::StagingBuffer<vsl::MemoryType::TransferDestination> full(device, sizeof(second));
        auto manager = make_manager();
        {
            auto phase = manager.startPhase<vsl::ComputePhase>();
            phase << vsl::command::ChangeImageBarrier(image, vsl::ImageLayout::TransferDstOptimal)
                  << vsl::command::CopyBufferToImage(image, &firstSource, vsl::ImageLayout::TransferDstOptimal)
                  << vsl::command::ImageReadback(&pixel, image, vsl::ImageLayout::TransferDstOptimal, {1, 0, 1, 1})
                  << vsl::command::CopyBufferToImage(image, &secondSource, vsl::ImageLayout::TransferDstOptimal)
                  << vsl::command::ImageReadback(full._data, image, vsl::ImageLayout::TransferDstOptimal);
            EXPECT_EQ(phase.state, vsl::ComputePhase::State::WaitSubmit);
        }
        device._data->device.waitIdle();
        auto mappedPixel = pixel.data(sizeof(std::uint32_t));
        std::uint32_t actualPixel;
        std::memcpy(&actualPixel, mappedPixel.data, sizeof(actualPixel));
        EXPECT_EQ(actualPixel, first[1]);
        auto mappedFull = full.data(sizeof(second));
        std::array<std::uint32_t, 4> actual;
        std::memcpy(actual.data(), mappedFull.data, sizeof(actual));
        EXPECT_EQ(actual, second);
    }

    TEST_F(ImageReadbackCommandVulkanIntegrationTest, SelectedLayerRestorationLeavesOtherLayersUsable) {
        const std::array<std::uint32_t, 4> expected{11, 22, 33, 44};
        TransferImage image(device, 2, 2, vsl::data_format::UnsignedInt, 2);
        vsl::StagingBuffer<> source(device, expected);
        vsl::StagingBuffer<vsl::MemoryType::TransferDestination> pixel(device, sizeof(std::uint32_t));
        vsl::StagingBuffer<vsl::MemoryType::TransferDestination> full(device, sizeof(expected));
        auto manager = make_manager();
        {
            auto phase = manager.startPhase<vsl::ComputePhase>();
            phase << vsl::command::ChangeImageBarrier(image, vsl::ImageLayout::TransferDstOptimal)
                  << vsl::command::CopyBufferToArrayImage(image, &source, vsl::ImageLayout::TransferDstOptimal, 0)
                  << vsl::command::CopyBufferToArrayImage(image, &source, vsl::ImageLayout::TransferDstOptimal, 1)
                  << vsl::command::ImageReadback(&pixel, image, vsl::ImageLayout::TransferDstOptimal, {1, 1, 1, 1, 1})
                  << vsl::command::CopyBufferToArrayImage(image, &source, vsl::ImageLayout::TransferDstOptimal, 1)
                  << vsl::command::ImageReadback(&full, image, vsl::ImageLayout::TransferDstOptimal, {0, 0, 2, 2, 0});
        }
        device._data->device.waitIdle();
        auto mapped = pixel.data(sizeof(std::uint32_t));
        std::uint32_t actual;
        std::memcpy(&actual, mapped.data, sizeof(actual));
        EXPECT_EQ(actual, expected[3]);
    }

    TEST_F(ImageReadbackCommandVulkanIntegrationTest, DescriptionAndInvalidContractsDoNotRecordCommands) {
        TransferImage image(device, 2, 2, vsl::data_format::UnsignedInt);
        const auto info = vsl::command::ImageReadback::describe(image, vsl::ImageLayout::General);
        EXPECT_EQ(info.region.width, 2u);
        EXPECT_EQ(info.region.height, 2u);
        EXPECT_EQ(info.rowStride, 8u);
        EXPECT_EQ(info.byteSize, 16u);
        EXPECT_EQ(info.format, vsl::data_format::UnsignedInt);
        vsl::StagingBuffer<vsl::MemoryType::TransferDestination> small(device, 4);
        auto manager = make_manager();
        auto invoke = [&](vsl::ImageLayout layout, vsl::ImageRegion region = {}) {
            vsl::command::ImageReadback(&small, image, layout, region)
                .invoke(manager.getPool(), manager.getBuffer(), manager);
        };
        EXPECT_THROW(invoke(vsl::ImageLayout::Undefined), std::invalid_argument);
        EXPECT_THROW(invoke(vsl::ImageLayout::Preinitialized), std::invalid_argument);
        EXPECT_THROW(invoke(vsl::ImageLayout::General), std::out_of_range);
        EXPECT_THROW(invoke(vsl::ImageLayout::General, {2, 0, 1, 1}), std::out_of_range);
    }
} // namespace

namespace {
    TEST_F(ImageReadbackCommandVulkanIntegrationTest, EmptyCommandBufferIsRejectedBeforeItIsDereferenced) {
        auto manager = make_manager();
        auto image = initialized_image(manager);
        vsl::StagingBuffer<vsl::MemoryType::TransferDestination> destination(device, std::size_t{16});
        const vsl::CommandBuffer empty(std::shared_ptr<vsl::_impl::CommandBuffer_impl>{});
        vsl::command::ImageReadback command(&destination, image, vsl::ImageLayout::TransferDstOptimal);
        EXPECT_THROW(command.invoke(manager.getPool(), empty, manager), std::invalid_argument);
    }
    TEST_F(ImageReadbackCommandVulkanIntegrationTest, EmptyManagerIsRejectedBeforeRetainingResourcesOrRecordingBarriers) {
        auto manager = make_manager();
        auto image = initialized_image(manager);
        vsl::StagingBuffer<vsl::MemoryType::TransferDestination> destination(device, std::size_t{16});
        const vsl::CommandManager empty(std::shared_ptr<vsl::_impl::CommandManager_impl>{});
        vsl::ComputePhase phase(manager);
        const auto buffer = manager.getBuffer();
        ASSERT_TRUE(buffer._data->recordingSubmission);
        const auto retained = buffer._data->recordingSubmission->resources;
        vsl::command::ImageReadback command(&destination, image, vsl::ImageLayout::TransferDstOptimal);
        EXPECT_THROW(command.invoke(manager.getPool(), buffer, empty), std::invalid_argument);
        EXPECT_EQ(buffer._data->recordingSubmission->resources, retained);
        phase.cancel();
    }
} // namespace
