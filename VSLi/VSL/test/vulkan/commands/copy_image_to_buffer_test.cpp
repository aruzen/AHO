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
        TransferImage image(device, 2, 2, vsl::data_format::UnsignedInt);
        vsl::StagingBuffer<> source(device, expected);
        vsl::StagingBuffer<vsl::MemoryType::TransferDestination> pixel(device, sizeof(std::uint32_t));
        vsl::StagingBuffer<vsl::MemoryType::TransferDestination> full(device, sizeof(expected));
        auto manager = make_manager();
        {
            auto phase = manager.startPhase<vsl::ComputePhase>();
            phase << vsl::command::ChangeImageBarrier(image, vsl::ImageLayout::TransferDstOptimal)
                  << vsl::command::CopyBufferToImage(image, &source, vsl::ImageLayout::TransferDstOptimal)
                  << vsl::command::ChangeImageBarrier(image, vsl::ImageLayout::TransferDstOptimal,
                                                      vsl::ImageLayout::TransferSrcOptimal)
                  << vsl::command::CopyImageToBuffer(pixel, image, vsl::ImageLayout::TransferSrcOptimal, {1, 0, 1, 1}, true)
                  << vsl::command::ChangeImageBarrier(image, vsl::ImageLayout::TransferSrcOptimal, vsl::ImageLayout::General)
                  << vsl::command::CopyImageToBuffer(&full, image, vsl::ImageLayout::General, {}, true);
            EXPECT_EQ(phase.state, vsl::ComputePhase::State::WaitSubmit);
            EXPECT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::Success);
        }
        device._data->device.waitIdle();
        auto mappedPixel = pixel.data(sizeof(std::uint32_t));
        std::uint32_t actualPixel;
        std::memcpy(&actualPixel, mappedPixel.data, sizeof(actualPixel));
        EXPECT_EQ(actualPixel, expected[1]);
        auto mappedFull = full.data(sizeof(expected));
        std::array<std::uint32_t, 4> actual;
        std::memcpy(actual.data(), mappedFull.data, sizeof(actual));
        EXPECT_EQ(actual, expected);
    }

    TEST_F(CopyImageToBufferVulkanIntegrationTest, LayerRangeLeavesOtherLayerLayoutAndTexelsUntouched) {
        const std::array<std::uint32_t, 4> first{11, 22, 33, 44}, second{55, 66, 77, 88};
        TransferImage image(device, 2, 2, vsl::data_format::UnsignedInt, 2);
        vsl::StagingBuffer<> firstSource(device, first), secondSource(device, second);
        vsl::StagingBuffer<vsl::MemoryType::TransferDestination> firstResult(device, sizeof(first));
        vsl::StagingBuffer<vsl::MemoryType::TransferDestination> secondResult(device, sizeof(std::uint32_t));
        auto manager = make_manager();
        {
            auto phase = manager.startPhase<vsl::ComputePhase>();
            phase << vsl::command::ChangeImageBarrier(image, vsl::ImageLayout::TransferDstOptimal)
                  << vsl::command::CopyBufferToArrayImage(image, &firstSource, vsl::ImageLayout::TransferDstOptimal, 0)
                  << vsl::command::CopyBufferToArrayImage(image, &secondSource, vsl::ImageLayout::TransferDstOptimal, 1)
                  << vsl::command::ChangeImageBarrier(image, vsl::ImageLayout::TransferDstOptimal, vsl::ImageLayout::TransferSrcOptimal)
                  << vsl::command::ChangeImageBarrier(image, vsl::ImageLayout::TransferSrcOptimal, vsl::ImageLayout::General)
                  << vsl::command::ChangeImageBarrier(image, vsl::ImageLayout::General, vsl::ImageLayout::TransferSrcOptimal, 1, 1)
                  << vsl::command::CopyImageToBuffer(&firstResult, image, vsl::ImageLayout::General, {}, true)
                  << vsl::command::CopyImageToBuffer(&secondResult, image, vsl::ImageLayout::TransferSrcOptimal, {1, 1, 1, 1, 1}, true)
                  << vsl::command::ChangeImageBarrier(image, vsl::ImageLayout::TransferSrcOptimal, vsl::ImageLayout::General, 1, 1);
        }
        device._data->device.waitIdle();
        auto mappedFirst = firstResult.data(sizeof(first));
        std::array<std::uint32_t, 4> actual;
        std::memcpy(actual.data(), mappedFirst.data, sizeof(actual));
        EXPECT_EQ(actual, first);
        auto mappedSecond = secondResult.data(sizeof(std::uint32_t));
        std::uint32_t value;
        std::memcpy(&value, mappedSecond.data, sizeof(value));
        EXPECT_EQ(value, second[3]);
    }

    TEST_F(CopyImageToBufferVulkanIntegrationTest, InvalidRegionsLayoutsAndBufferSizesAreRejectedBeforeRecording) {
        TransferImage image(device, 2, 2, vsl::data_format::UnsignedInt);
        vsl::StagingBuffer<vsl::MemoryType::TransferDestination> small(device, sizeof(std::uint32_t));
        auto manager = make_manager();
        auto invoke = [&](vsl::ImageLayout layout, vsl::ImageRegion region) {
            vsl::command::CopyImageToBuffer(&small, image, layout, region, true)
                .invoke(manager.getPool(), manager.getBuffer(), manager);
        };
        EXPECT_THROW(invoke(vsl::ImageLayout::General, {}), std::out_of_range);
        EXPECT_THROW(invoke(vsl::ImageLayout::General, {2, 0, 1, 1}), std::out_of_range);
        EXPECT_THROW(invoke(vsl::ImageLayout::General, {0, 0, 1, 1, 1}), std::out_of_range);
        EXPECT_THROW(invoke(vsl::ImageLayout::General, {1, 0, 0, 0}), std::invalid_argument);
        EXPECT_THROW(invoke(vsl::ImageLayout::ColorAttachmentOptimal, {0, 0, 1, 1}), std::invalid_argument);
    }
} // namespace
