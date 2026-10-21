#include "support/image_transfer_fixture.hpp"
#include <VSL/vulkan/commands/change_image_barrier.hpp>
namespace {
    class CopyImageToBufferVulkanIntegrationTest : public vsl_test::ImageTransferVulkanIntegrationTest {
        bool validate_vulkan() const override;
    };

    bool CopyImageToBufferVulkanIntegrationTest::validate_vulkan() const {
        return true;
    }
    TEST_F(CopyImageToBufferVulkanIntegrationTest, TypedAndAccessorOverloadsPreserveEveryTexel) {
        const std::array<std::uint32_t, 4> expected{0x12345678, 0x90abcdef, 0, 0xffffffff};
        EXPECT_EQ(round_trip(false, false), expected);
        EXPECT_EQ(round_trip(true, false), expected);
    }

    TEST_F(CopyImageToBufferVulkanIntegrationTest, RegionAndHostVisibilityComposeWithExistingCommands) {
        const std::array<std::uint32_t, 4> expected{11, 22, 33, 44};
        TransferImage image(device, 2, 2, vsl::data_format::UNSIGNED_INT);
        vsl::StagingBuffer<> source(device, expected);
        vsl::StagingBuffer<vsl::MemoryType::TRANSFER_DESTINATION> pixel(device, sizeof(std::uint32_t));
        vsl::StagingBuffer<vsl::MemoryType::TRANSFER_DESTINATION> full(device, sizeof(expected));
        auto manager = make_manager();
        {
            auto phase = manager.start_phase<vsl::ComputePhase>();
            phase << vsl::command::ChangeImageBarrier(image, vsl::ImageLayout::TRANSFER_DST_OPTIMAL)
                  << vsl::command::CopyBufferToImage(image, &source, vsl::ImageLayout::TRANSFER_DST_OPTIMAL)
                  << vsl::command::ChangeImageBarrier(image, vsl::ImageLayout::TRANSFER_DST_OPTIMAL,
                                                      vsl::ImageLayout::TRANSFER_SRC_OPTIMAL)
                  << vsl::command::CopyImageToBuffer(pixel, image, vsl::ImageLayout::TRANSFER_SRC_OPTIMAL, {1, 0, 1, 1}, true)
                  << vsl::command::ChangeImageBarrier(image, vsl::ImageLayout::TRANSFER_SRC_OPTIMAL, vsl::ImageLayout::GENERAL)
                  << vsl::command::CopyImageToBuffer(&full, image, vsl::ImageLayout::GENERAL, {}, true);
            EXPECT_EQ(phase.state, vsl::ComputePhase::State::WAIT_SUBMIT);
            EXPECT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
        }
        device.data_->device.waitIdle();
        auto mapped_pixel = pixel.data(sizeof(std::uint32_t));
        std::uint32_t actual_pixel;
        std::memcpy(&actual_pixel, mapped_pixel.data, sizeof(actual_pixel));
        EXPECT_EQ(actual_pixel, expected[1]);
        auto mapped_full = full.data(sizeof(expected));
        std::array<std::uint32_t, 4> actual;
        std::memcpy(actual.data(), mapped_full.data, sizeof(actual));
        EXPECT_EQ(actual, expected);
    }

    TEST_F(CopyImageToBufferVulkanIntegrationTest, LayerRangeLeavesOtherLayerLayoutAndTexelsUntouched) {
        const std::array<std::uint32_t, 4> first{11, 22, 33, 44}, second{55, 66, 77, 88};
        TransferImage image(device, 2, 2, vsl::data_format::UNSIGNED_INT, 2);
        vsl::StagingBuffer<> first_source(device, first), second_source(device, second);
        vsl::StagingBuffer<vsl::MemoryType::TRANSFER_DESTINATION> first_result(device, sizeof(first));
        vsl::StagingBuffer<vsl::MemoryType::TRANSFER_DESTINATION> second_result(device, sizeof(std::uint32_t));
        auto manager = make_manager();
        {
            auto phase = manager.start_phase<vsl::ComputePhase>();
            phase << vsl::command::ChangeImageBarrier(image, vsl::ImageLayout::TRANSFER_DST_OPTIMAL)
                  << vsl::command::CopyBufferToArrayImage(image, &first_source, vsl::ImageLayout::TRANSFER_DST_OPTIMAL, 0)
                  << vsl::command::CopyBufferToArrayImage(image, &second_source, vsl::ImageLayout::TRANSFER_DST_OPTIMAL, 1)
                  << vsl::command::ChangeImageBarrier(image, vsl::ImageLayout::TRANSFER_DST_OPTIMAL, vsl::ImageLayout::TRANSFER_SRC_OPTIMAL)
                  << vsl::command::ChangeImageBarrier(image, vsl::ImageLayout::TRANSFER_SRC_OPTIMAL, vsl::ImageLayout::GENERAL)
                  << vsl::command::ChangeImageBarrier(image, vsl::ImageLayout::GENERAL, vsl::ImageLayout::TRANSFER_SRC_OPTIMAL, 1, 1)
                  << vsl::command::CopyImageToBuffer(&first_result, image, vsl::ImageLayout::GENERAL, {}, true)
                  << vsl::command::CopyImageToBuffer(&second_result, image, vsl::ImageLayout::TRANSFER_SRC_OPTIMAL, {1, 1, 1, 1, 1}, true)
                  << vsl::command::ChangeImageBarrier(image, vsl::ImageLayout::TRANSFER_SRC_OPTIMAL, vsl::ImageLayout::GENERAL, 1, 1);
        }
        device.data_->device.waitIdle();
        auto mapped_first = first_result.data(sizeof(first));
        std::array<std::uint32_t, 4> actual;
        std::memcpy(actual.data(), mapped_first.data, sizeof(actual));
        EXPECT_EQ(actual, first);
        auto mapped_second = second_result.data(sizeof(std::uint32_t));
        std::uint32_t value;
        std::memcpy(&value, mapped_second.data, sizeof(value));
        EXPECT_EQ(value, second[3]);
    }

    TEST_F(CopyImageToBufferVulkanIntegrationTest, InvalidRegionsLayoutsAndBufferSizesAreRejectedBeforeRecording) {
        TransferImage image(device, 2, 2, vsl::data_format::UNSIGNED_INT);
        vsl::StagingBuffer<vsl::MemoryType::TRANSFER_DESTINATION> small(device, sizeof(std::uint32_t));
        auto manager = make_manager();
        auto invoke = [&](vsl::ImageLayout layout, vsl::ImageRegion region) {
            vsl::command::CopyImageToBuffer(&small, image, layout, region, true)
                .invoke(manager.get_pool(), manager.get_buffer(), manager);
        };
        EXPECT_THROW(invoke(vsl::ImageLayout::GENERAL, {}), std::out_of_range);
        EXPECT_THROW(invoke(vsl::ImageLayout::GENERAL, {2, 0, 1, 1}), std::out_of_range);
        EXPECT_THROW(invoke(vsl::ImageLayout::GENERAL, {0, 0, 1, 1, 1}), std::out_of_range);
        EXPECT_THROW(invoke(vsl::ImageLayout::GENERAL, {1, 0, 0, 0}), std::invalid_argument);
        EXPECT_THROW(invoke(vsl::ImageLayout::COLOR_ATTACHMENT_OPTIMAL, {0, 0, 1, 1}), std::invalid_argument);
    }
} // namespace
