#include <VSL/lifetime/command_access.hpp>
#include "support/vulkan_fixture.hpp"
#include <VSL/lifetime/native_recording.hpp>
#include <VSL/lifetime/buffer_arena.hpp>
#include <optional>

namespace {
    using VSL_NAMESPACE::lifetime::CommandAccess;
}
namespace {
    class NativeRecordingVulkanIntegrationTest : public vsl_test::VulkanIntegrationTest {
        bool validate_vulkan() const override { return true; }
    };

    TEST_F(NativeRecordingVulkanIntegrationTest, IdleCancelledSubmittedAndReusedSlotsRejectOldContexts) {
        auto manager = make_manager().independent(1);
        EXPECT_THROW(vsl::NativeRecordingContext(manager.get_buffer()), std::logic_error);
        auto phase = manager.start_phase<vsl::ComputePhase>();
        vsl::NativeRecordingContext first(manager.get_buffer());
        EXPECT_EQ(first.completion(), phase.completion());
        EXPECT_EQ(first.slot(), 0u);
        EXPECT_NE(first.native_command_buffer(), VK_NULL_HANDLE);
        vsl::NativeRecordingContext moved(std::move(first));
        EXPECT_THROW(first.require_current(), std::logic_error);
        phase.cancel();
        EXPECT_THROW(moved.require_current(), std::logic_error);
        EXPECT_THROW(moved.native_command_buffer(), std::logic_error);
        auto next = manager.start_phase<vsl::ComputePhase>();
        vsl::NativeRecordingContext current(manager.get_buffer());
        EXPECT_EQ(current.slot(), moved.slot());
        EXPECT_NE(current.completion(), moved.completion());
        EXPECT_THROW(moved.use({}), std::logic_error);
        EXPECT_THROW(moved.retain({}), std::logic_error);
        EXPECT_NO_THROW(current.require_current());
        ASSERT_EQ(next.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
        EXPECT_THROW(current.native_command_buffer(), std::logic_error);
        ASSERT_TRUE(current.completion().wait());
        EXPECT_EQ(current.completion().status(), vsl::Completion::Status::COMPLETE);
    }

    TEST_F(NativeRecordingVulkanIntegrationTest, DifferentControlBlocksCannotImpersonateTheCommandRingOwner) {
        auto manager = make_manager();
        auto phase = manager.start_phase<vsl::ComputePhase>();
        auto buffer = manager.get_buffer();
        const auto token = std::make_shared<int>(0);
        CommandAccess::Data(buffer) = std::shared_ptr<vsl::impl::CommandBufferImpl>(token, CommandAccess::Data(buffer).get());
        EXPECT_THROW((vsl::NativeRecordingContext{buffer}), std::logic_error);
        EXPECT_NO_THROW(vsl::NativeRecordingContext(manager.get_buffer()));
        phase.cancel();
    }

    TEST_F(NativeRecordingVulkanIntegrationTest, ExplicitUseAndStrongNullOwnersFollowTheSelectedRetention) {
        const std::array<vsl::BufferCreateInfo, 2> infos{{{4, vsl::MemoryType::TRANSFER_SOURCE}, {4, vsl::MemoryType::TRANSFER_SOURCE}}};
        for (const auto retention : {vsl::ResourceRetention::COMPLETION, vsl::ResourceRetention::SUBMISSION}) {
            for (const bool cancel : {false, true}) {
                std::weak_ptr<void> storage;
                std::weak_ptr<int> retained;
                {
                    auto manager = make_manager().independent(1);
                    auto phase = manager.start_phase<vsl::ComputePhase>();
                    vsl::NativeRecordingContext recording(manager.get_buffer());
                    {
                        vsl::lifetime::BufferArena arena(device, infos);
                        storage = arena.owner();
                        const auto token = std::make_shared<int>(7);
                        retained = token;
                        recording.use(arena.view(0).resource_use(), retention);
                        recording.use(arena.view(1).resource_use(), retention);
                        recording.retain(arena.view(0).owner_ref(), retention);
                        recording.retain(std::shared_ptr<void>(token, nullptr), retention);
                        vkCmdPipelineBarrier(recording.native_command_buffer(), VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                                             0, 0, nullptr, 0, nullptr, 0, nullptr);
                    }
                    EXPECT_FALSE(storage.expired());
                    EXPECT_FALSE(retained.expired());
                    if (cancel)
                        phase.cancel();
                    else {
                        ASSERT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
                        ASSERT_TRUE(phase.completion().wait());
                    }
                    EXPECT_EQ(storage.expired(), retention == vsl::ResourceRetention::COMPLETION);
                    EXPECT_EQ(retained.expired(), retention == vsl::ResourceRetention::COMPLETION);
                    EXPECT_THROW(recording.require_current(), std::logic_error);
                }
                EXPECT_TRUE(storage.expired());
                EXPECT_TRUE(retained.expired());
            }
        }
    }

    TEST_F(NativeRecordingVulkanIntegrationTest, ResourcePollingCanDestroyTheContextAndContinueCancelOrStartAPeer) {
        for (const auto action : {0, 1, 2}) {
            auto manager = make_manager().independent(1), other = manager.independent(1);
            vsl::Buffer<vsl::MemoryType::TRANSFER_SOURCE, vsl::MemoryProperty::HOST_VISIBLE> source(device, 4);
            std::optional<vsl::ComputePhase> recording, peer;
            std::optional<vsl::NativeRecordingContext> context;
            bool released = false;
            auto submitted = manager.start_phase<vsl::ComputePhase>();
            source.resource_use().record(manager.get_buffer());
            manager.get_buffer().retain_until_complete(std::shared_ptr<int>(new int, [&](int* value) {
                delete value;
                released = true;
                context.reset();
                if (action) {
                    recording->cancel();
                    if (action == 2)
                        peer.emplace(other);
                }
            }));
            ASSERT_EQ(submitted.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
            device.data_->device.waitIdle();
            recording.emplace(other);
            context.emplace(other.get_buffer());
            if (action)
                EXPECT_THROW(context->use(source.resource_use()), std::logic_error);
            else
                EXPECT_NO_THROW(context->use(source.resource_use()));
            EXPECT_TRUE(released);
            EXPECT_FALSE(context);
            if (action == 1)
                EXPECT_THROW(vsl::NativeRecordingContext(other.get_buffer()), std::logic_error);
            else {
                const vsl::NativeRecordingContext current(other.get_buffer());
                EXPECT_NE(current.native_command_buffer(), VK_NULL_HANDLE);
            }
            if (peer)
                peer->cancel();
            recording->cancel();
        }
    }
} // namespace
