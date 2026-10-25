#include "support/image_transfer_fixture.hpp"
namespace {
    TEST(ChangeImageBarrierTest, UnspecifiedAndExplicitShaderStagesPreserveTheirScopeAndLayers) {
        using namespace vsl;
        command::ChangeImageBarrier automatic({}, ImageLayout::TRANSFER_DST_OPTIMAL, ImageLayout::SHADER_READ_ONLY_OPTIMAL);
        EXPECT_FALSE(automatic.shader_read_stages());
        const auto stages = ShaderFlag::VERTEX | ShaderFlag::COMPUTE;
        command::ChangeImageBarrier explicit_stages({}, ImageLayout::TRANSFER_DST_OPTIMAL, ImageLayout::SHADER_READ_ONLY_OPTIMAL, stages);
        EXPECT_EQ(explicit_stages.shader_read_stages(), stages);
        command::ChangeImageBarrier layers({}, ImageLayout::TRANSFER_DST_OPTIMAL, ImageLayout::SHADER_READ_ONLY_OPTIMAL, 2, 3, stages);
        EXPECT_EQ(layers.shader_read_stages(), stages);
        EXPECT_EQ(layers.base_layer, 2u);
        EXPECT_EQ(layers.layer_count, 3u);
    }

    TEST(ChangeImageBarrierTest, RejectsEmptyUnknownStagesAndTransitionsWithoutShaderReadDestination) {
        using namespace vsl;
        for (const auto stages : {ShaderFlag::ERROR_, static_cast<ShaderFlag>(0x40000000),
                                  ShaderFlag::COMPUTE | static_cast<ShaderFlag>(0x40000000)}) {
            EXPECT_THROW((command::ChangeImageBarrier({}, ImageLayout::TRANSFER_DST_OPTIMAL,
                                                      ImageLayout::SHADER_READ_ONLY_OPTIMAL, stages)),
                         std::invalid_argument);
        }
        EXPECT_THROW((command::ChangeImageBarrier({}, ImageLayout::UNDEFINED, ImageLayout::TRANSFER_DST_OPTIMAL, ShaderFlag::FRAGMENT)),
                     std::invalid_argument);
        EXPECT_THROW((command::ChangeImageBarrier({}, ImageLayout::GENERAL, ImageLayout::SHADER_READ_ONLY_OPTIMAL, ShaderFlag::COMPUTE)),
                     std::invalid_argument);
    }

    class ChangeImageBarrierVulkanIntegrationTest : public vsl_test::ImageTransferVulkanIntegrationTest {
        bool validate_vulkan() const override { return true; }
    };
    TEST_F(ChangeImageBarrierVulkanIntegrationTest, RetainsExplicitAndTypedLayoutsAndRejectsUnsupportedPair) {
        TransferImage image(device, 2, 2, vsl::data_format::UNSIGNED_INT);
        vsl::command::ChangeImageBarrier typed(image, vsl::ImageLayout::TRANSFER_DST_OPTIMAL);
        EXPECT_EQ(typed.old_layout, vsl::ImageLayout::UNDEFINED);
        EXPECT_EQ(typed.new_layout, vsl::ImageLayout::TRANSFER_DST_OPTIMAL);
        EXPECT_EQ(typed.image.data_, image.data_);
        vsl::command::ChangeImageBarrier unsupported(image, vsl::ImageLayout::GENERAL,
                                                     vsl::ImageLayout::UNDEFINED);
        auto manager = make_manager();
        EXPECT_THROW(unsupported.invoke(manager.get_pool(), manager.get_buffer(), manager), std::invalid_argument);
    }
    TEST_F(ChangeImageBarrierVulkanIntegrationTest, TransitionsTransferDestinationToShaderRead) {
        TransferImage image(device, 2, 2, vsl::data_format::UNSIGNED_INT);
        auto manager = make_manager();
        {
            auto phase = manager.start_phase<vsl::ComputePhase>();
            transition(manager, image, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal, {},
                       vk::AccessFlagBits::eTransferWrite);
            phase << vsl::command::ChangeImageBarrier(image, vsl::ImageLayout::TRANSFER_DST_OPTIMAL,
                                                      vsl::ImageLayout::SHADER_READ_ONLY_OPTIMAL);
            EXPECT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
        }
        EXPECT_NO_THROW(device.data_->device.waitIdle());
    }

    TEST_F(ChangeImageBarrierVulkanIntegrationTest, RejectsLayerRangesOutsideTheImageBeforeRecording) {
        TransferImage image(device, 2, 2, vsl::data_format::UNSIGNED_INT, 2);
        auto manager = make_manager();
        auto invoke = [&](std::uint32_t base, std::uint32_t count) {
            vsl::command::ChangeImageBarrier(image, vsl::ImageLayout::GENERAL,
                                             vsl::ImageLayout::TRANSFER_SRC_OPTIMAL, base, count)
                .invoke(manager.get_pool(), manager.get_buffer(), manager);
        };
        EXPECT_THROW(invoke(2, 0), std::out_of_range);
        EXPECT_THROW(invoke(1, 2), std::out_of_range);
        EXPECT_THROW(invoke(UINT32_MAX, UINT32_MAX), std::out_of_range);
    }

    TEST_F(ChangeImageBarrierVulkanIntegrationTest, RejectsLayoutMutationThatWouldDiscardExplicitShaderStages) {
        TransferImage image(device, 2, 2, vsl::data_format::UNSIGNED_INT);
        auto manager = make_manager();
        vsl::command::ChangeImageBarrier barrier(image, vsl::ImageLayout::TRANSFER_DST_OPTIMAL,
                                                 vsl::ImageLayout::SHADER_READ_ONLY_OPTIMAL, vsl::ShaderFlag::COMPUTE);
        barrier.new_layout = vsl::ImageLayout::TRANSFER_SRC_OPTIMAL;
        EXPECT_THROW(barrier.invoke(manager.get_pool(), manager.get_buffer(), manager), std::invalid_argument);
    }
} // namespace
