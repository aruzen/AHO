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
        void invoke(vsl::DefaultPhaseStream& stream, vsl::CommandPool, vsl::CommandBuffer,
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

namespace {
    struct VertexCountSource {
        std::size_t count;
        std::size_t getVertexSize() { return count; }
        void invoke(vsl::CommandPool, vsl::CommandBuffer, vsl::CommandManager) {}
    };
    struct VertexCountConsumer {
        std::shared_ptr<std::optional<std::size_t>> received;
        void setVertexSize(std::optional<std::size_t> value) { *received = value; }
        void invoke(vsl::CommandPool, vsl::CommandBuffer, vsl::CommandManager) {}
    };
    struct VertexCountDelegate : vsl::command::__DelegateCommand, vsl::command::__VertexSizeRequire {
        std::optional<std::size_t> received;
        void setVertexSize(std::optional<std::size_t> value) override { received = value; }
        void invoke(vsl::DefaultPhaseStream&, vsl::CommandPool, vsl::CommandBuffer, vsl::CommandManager) override {}
    };
    TEST_F(PhaseVulkanIntegrationTest, ValueCommandsExportAndReceiveVertexCountsThroughTheirOwnRoles) {
        auto manager = make_manager();
        auto received = std::make_shared<std::optional<std::size_t>>(99);
        vsl::DefaultPhaseStream stream{manager};
        stream << VertexCountConsumer{received};
        EXPECT_FALSE(*received);
        stream << VertexCountSource{17} << VertexCountConsumer{received};
        EXPECT_EQ(stream.vertexSize, 17u);
        EXPECT_EQ(*received, 17u);
        stream << VertexCountSource{0} << VertexCountConsumer{received};
        EXPECT_EQ(*received, 0u);
    }
    TEST_F(PhaseVulkanIntegrationTest, ValueAndSharedDelegatesReceiveTheSameVertexCount) {
        auto manager = make_manager();
        vsl::DefaultPhaseStream stream{manager, 23};
        struct ValueDelegate {
            std::shared_ptr<std::optional<std::size_t>> received;
            void setVertexSize(std::optional<std::size_t> value) { *received = value; }
            void invoke(vsl::DefaultPhaseStream&, vsl::CommandPool, vsl::CommandBuffer, vsl::CommandManager) {}
        };
        auto received = std::make_shared<std::optional<std::size_t>>();
        stream << ValueDelegate{received};
        EXPECT_EQ(*received, 23u);
        auto shared = std::make_shared<VertexCountDelegate>();
        stream << std::shared_ptr<vsl::command::__DelegateCommand>(shared);
        EXPECT_EQ(shared->received, *received);
    }
} // namespace

namespace {
    struct IncrementVertexCount : vsl::command::__Command,
                                  vsl::command::__VertexSizeRequire,
                                  vsl::command::__VertexSizeHolder {
        std::optional<std::size_t> input;
        void setVertexSize(std::optional<std::size_t> count) override { input = count; }
        std::size_t getVertexSize() override { return input.value_or(0) + 1; }
        void invoke(vsl::CommandPool, vsl::CommandBuffer, vsl::CommandManager) override {}
    };
    TEST_F(PhaseVulkanIntegrationTest, CommandsConsumeThePreviousCountBeforeExportingTheirOwnCount) {
        auto manager = make_manager();
        vsl::DefaultPhaseStream value_stream{manager, 17};
        value_stream << IncrementVertexCount{};
        EXPECT_EQ(value_stream.vertexSize, 18u);
        auto command = std::make_shared<IncrementVertexCount>();
        vsl::DefaultPhaseStream shared_stream{manager, 17};
        shared_stream << std::shared_ptr<vsl::command::__Command>(command);
        EXPECT_EQ(command->input, 17u);
        EXPECT_EQ(shared_stream.vertexSize, 18u);
        EXPECT_EQ(value_stream.vertexSize, shared_stream.vertexSize);
    }
} // namespace
