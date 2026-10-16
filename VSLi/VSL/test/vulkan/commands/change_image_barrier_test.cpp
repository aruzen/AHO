#include "support/image_transfer_fixture.hpp"
namespace {
using ChangeImageBarrierVulkanIntegrationTest = vsl_test::ImageTransferVulkanIntegrationTest;
TEST_F(ChangeImageBarrierVulkanIntegrationTest, RetainsExplicitAndTypedLayoutsAndRejectsUnsupportedPair) {
    TransferImage image(device, 2, 2, vsl::data_format::UnsignedInt);
    vsl::command::ChangeImageBarrier typed(image, vsl::ImageLayout::TransferDstOptimal);
    EXPECT_EQ(typed.oldLayout, vsl::ImageLayout::Undefined);
    EXPECT_EQ(typed.newLayout, vsl::ImageLayout::TransferDstOptimal);
    EXPECT_EQ(typed.image._data, image._data);
    vsl::command::ChangeImageBarrier unsupported(image, vsl::ImageLayout::General,
                                                 vsl::ImageLayout::Undefined);
    auto manager = make_manager();
    EXPECT_THROW(unsupported.invoke(manager.getPool(), manager.getBuffer(), manager), std::invalid_argument);
}
TEST_F(ChangeImageBarrierVulkanIntegrationTest, TransitionsTransferDestinationToShaderRead) {
    TransferImage image(device, 2, 2, vsl::data_format::UnsignedInt);
    auto manager = make_manager();
    {
        auto phase = manager.startPhase<vsl::ComputePhase>();
        transition(manager, image, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal, {},
                   vk::AccessFlagBits::eTransferWrite);
        phase << vsl::command::ChangeImageBarrier(image, vsl::ImageLayout::TransferDstOptimal,
                                                  vsl::ImageLayout::ShaderReadOnlyOptimal);
        EXPECT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::Success);
    }
    EXPECT_NO_THROW(device._data->device.waitIdle());
    // Exact access/stage masks require a validation capture; see coverage document.
}
} // namespace
