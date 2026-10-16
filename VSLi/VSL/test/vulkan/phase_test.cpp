#include "support/vulkan_fixture.hpp"
namespace {
struct RecordCommand : vsl::command::__Command {
    std::shared_ptr<int> calls;
    explicit RecordCommand(std::shared_ptr<int> calls) : calls(std::move(calls)) {}
    void invoke(vsl::CommandPool, vsl::CommandBuffer, vsl::CommandManager) override {
        ++*calls;
    }
};
struct RecordDelegate : vsl::command::__DelegateCommand {
    std::shared_ptr<int> calls;
    explicit RecordDelegate(std::shared_ptr<int> calls) : calls(std::move(calls)) {}
    void invoke(vsl::DefaultPhaseStream &stream, vsl::CommandPool, vsl::CommandBuffer,
                vsl::CommandManager manager) override {
        EXPECT_EQ(stream.manager._data, manager._data);
        ++*calls;
    }
};
using PhaseVulkanIntegrationTest = vsl_test::VulkanIntegrationTest;
TEST_F(PhaseVulkanIntegrationTest, PostponedSetupAndSubmissionFollowStateTransitions) {
    auto manager = make_manager();
    vsl::ComputePhase phase(manager, std::nullopt, std::nullopt, std::nullopt, true);
    EXPECT_EQ(phase.state, vsl::ComputePhase::State::BeforeSetup);
    EXPECT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::Failed);
    phase.setup();
    EXPECT_EQ(phase.state, vsl::ComputePhase::State::WaitSubmit);
    phase.setup();
    EXPECT_EQ(phase.state, vsl::ComputePhase::State::WaitSubmit);
    EXPECT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::Success);
    EXPECT_EQ(phase.state, vsl::ComputePhase::State::FinishSubmit);
    EXPECT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::Failed);
    device._data->device.waitIdle();
}
TEST_F(PhaseVulkanIntegrationTest, StreamsInvokeValueAndSharedCommandsAndDelegates) {
    auto calls = std::make_shared<int>(0);
    auto manager = make_manager();
    {
        auto phase = manager.startPhase<vsl::ComputePhase>();
        auto stream = phase << RecordCommand(calls);
        EXPECT_EQ(*calls, 1);
        stream << RecordCommand(calls) << RecordDelegate(calls);
        EXPECT_EQ(*calls, 3);
        stream << std::shared_ptr<vsl::command::__Command>(std::make_shared<RecordCommand>(calls));
        stream << std::shared_ptr<vsl::command::__DelegateCommand>(std::make_shared<RecordDelegate>(calls));
        EXPECT_EQ(*calls, 5);
        phase << std::shared_ptr<vsl::command::__Command>(std::make_shared<RecordCommand>(calls));
        phase << std::shared_ptr<vsl::command::__DelegateCommand>(std::make_shared<RecordDelegate>(calls));
        phase << RecordDelegate(calls);
        EXPECT_EQ(*calls, 8);
    }
    device._data->device.waitIdle();
}
TEST_F(PhaseVulkanIntegrationTest, PostponedDefaultPhaseCanBeInspectedWithoutAcquiringAnImage) {
    auto manager = make_manager();
    vsl::DefaultPhase phase(manager, vsl::SwapchainAccessor{}, std::nullopt, std::nullopt, std::nullopt,
                            true);
    EXPECT_EQ(phase.state, vsl::ComputePhase::State::BeforeSetup);
    EXPECT_EQ(phase.getCurrentIndex(), manager.getCurrentBufferIdx());
    EXPECT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::Failed);
    // getImageIndex is excluded before acquire: it reads an uninitialized member.
}
} // namespace
