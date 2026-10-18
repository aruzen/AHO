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
        EXPECT_EQ(phase.getImageIndex(), UINT32_MAX);
    }
} // namespace

namespace {
    class PhaseCompletionVulkanIntegrationTest : public vsl_test::VulkanIntegrationTest {
        bool validate_vulkan() const override { return true; }
    };
    TEST_F(PhaseCompletionVulkanIntegrationTest, RepeatedSubmissionsReuseBuffersWithoutExternalFenceOrIdle) {
        auto manager = make_manager();
        vsl::Completion previous;
        for (unsigned i = 0; i < 20; ++i) {
            vsl::ComputePhase phase(manager);
            if (previous)
                EXPECT_TRUE(previous.ready());
            auto token = phase.completion();
            EXPECT_EQ(token.status(), vsl::Completion::Status::Pending);
            EXPECT_FALSE(token.wait(0));
            EXPECT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::Success);
            previous = token;
        }
        ASSERT_TRUE(previous.wait());
        EXPECT_EQ(previous.status(), vsl::Completion::Status::Complete);
    }
    TEST_F(PhaseCompletionVulkanIntegrationTest, CancellationAndNestedRecordingDoNotSubmitOrResetActiveBuffer) {
        auto manager = make_manager();
        vsl::ComputePhase first(manager);
        EXPECT_THROW((vsl::ComputePhase(manager)), std::logic_error);
        auto cancelled = first.completion();
        first.cancel();
        EXPECT_EQ(cancelled.status(), vsl::Completion::Status::Cancelled);
        EXPECT_FALSE(cancelled.wait(0));
        EXPECT_EQ(first.submit(), vsl::ComputePhase::SubmitResult::Failed);
        vsl::ComputePhase next(manager);
        EXPECT_EQ(next.submit(), vsl::ComputePhase::SubmitResult::Success);
        EXPECT_TRUE(next.completion().wait());
    }
    TEST_F(PhaseCompletionVulkanIntegrationTest, OldCompletionRemainsCompleteWhenNativeFenceIsReused) {
        auto manager = make_manager();
        vsl::ComputePhase first(manager);
        auto original = first.completion();
        first.submit();
        vsl::ComputePhase second(manager);
        EXPECT_TRUE(original.ready());
        second.submit();
        EXPECT_TRUE(original.wait(0));
        EXPECT_TRUE(second.completion().wait());
    }
    TEST_F(PhaseCompletionVulkanIntegrationTest, RetainedResourcesSurviveUntilCompletionTokenIsReleased) {
        auto manager = make_manager();
        std::weak_ptr<int> resource;
        {
            vsl::ComputePhase phase(manager);
            auto retained = std::make_shared<int>(7);
            resource = retained;
            phase.completion().retain(retained);
            retained.reset();
            EXPECT_FALSE(resource.expired());
            phase.submit();
            EXPECT_TRUE(phase.completion().wait());
        }
        {
            vsl::ComputePhase next(manager);
            next.cancel();
        }
        // The old slot still owns its last submission until a new submission replaces it.
        {
            vsl::ComputePhase next(manager);
            next.submit();
            next.completion().wait();
        }
        EXPECT_TRUE(resource.expired());
    }
}

#include "support/window_fixture.hpp"
#include <VSL/vulkan/commands/image_readback.hpp>
namespace {
    class PresentPhaseWindowIntegrationTest : public vsl_test::WindowIntegrationTest {
        bool validate_vulkan() const override { return true; }
    };
    TEST_F(PresentPhaseWindowIntegrationTest, UsesAcquiredImageForFramebufferReadbackAndPresentAcrossFrames) {
        make_surface();
        ASSERT_FALSE(HasFatalFailure());
        if (!physical._data->device.getSurfaceSupportKHR(queue_family, surface->_data->surface))
            GTEST_SKIP() << "Graphics queue cannot present";
        if (!(physical._data->device.getSurfaceCapabilitiesKHR(surface->_data->surface).supportedUsageFlags & vk::ImageUsageFlagBits::eTransferSrc))
            GTEST_SKIP() << "Surface images cannot be read back";
        vsl::Swapchain chain(device, surface, std::nullopt, 64, 48,
                             vsl::ImageType::ColorAttachment | vsl::ImageType::TransferSrc);
        vsl::RenderPass pass(chain);
        vsl::FrameBuffer target(chain, pass);
        auto manager = make_manager();
        auto bytes = std::size_t(chain._data->extent.width) * chain._data->extent.height * 4;
        vsl::StagingBuffer<vsl::MemoryType::TransferDestination> destination(device, bytes);
        bool differentIndex = false;
        for (unsigned frame = 0; frame < 12; ++frame) {
            vsl::DefaultPhase phase(manager, chain); // no user-supplied semaphores or fence
            auto index = phase.getImageIndex();
            differentIndex |= index != phase.getCurrentIndex();
            auto begin = vsl::command::RenderPassBegin(pass, target);
            begin.clearValues[0] = vsl::ClearColor{1.f, 0.f, 0.f, 1.f};
            phase << begin << vsl::command::RenderPassEnd{}
                  << vsl::command::ImageReadback(&destination, chain.images.at(index), vsl::ImageLayout::Present);
            EXPECT_EQ(phase.submitCommands(), vsl::ComputePhase::SubmitResult::Success);
            EXPECT_EQ(phase.present(), vsl::ComputePhase::SubmitResult::Success);
            ASSERT_TRUE(phase.completion().wait());
            auto mapped = destination.data();
            const bool bgra = chain.getFormat().type() == vsl::data_format::___FormatTypes::Srgb8BGRA ||
                              chain.getFormat().type() == vsl::data_format::___FormatTypes::UnsignedNormalized8BGRA;
            const auto *pixel = static_cast<const unsigned char *>(mapped.data);
            EXPECT_EQ(pixel[bgra ? 2 : 0], 255);
            EXPECT_EQ(pixel[1], 0);
            EXPECT_EQ(pixel[3], 255);
            EXPECT_EQ(phase.present(), vsl::ComputePhase::SubmitResult::Failed);
        }
        // Implementations may acquire in frame-slot order; correctness does not rely on that order.
        (void)differentIndex;
        device._data->device.waitIdle();
    }
    TEST_F(PresentPhaseWindowIntegrationTest, CancelledAcquiredFrameCanBeFollowedByAnotherFrame) {
        make_surface();
        ASSERT_FALSE(HasFatalFailure());
        if (!physical._data->device.getSurfaceSupportKHR(queue_family, surface->_data->surface))
            GTEST_SKIP() << "Graphics queue cannot present";
        vsl::Swapchain chain(device, surface, std::nullopt, 64, 48);
        vsl::RenderPass pass(chain);
        vsl::FrameBuffer target(chain, pass);
        auto manager = make_manager();
        {
            vsl::DefaultPhase phase(manager, chain);
            auto discarded = phase.completion();
            phase << vsl::command::RenderPassBegin(pass, target); // intentionally no End
            phase.cancel();
            EXPECT_EQ(discarded.status(), vsl::Completion::Status::Cancelled);
            EXPECT_TRUE(phase.completion().wait());
        }
        {
            vsl::DefaultPhase phase(manager, chain);
            phase << vsl::command::RenderPassBegin(pass, target) << vsl::command::RenderPassEnd{};
            EXPECT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::Success);
            EXPECT_TRUE(phase.completion().wait());
        }
        device._data->device.waitIdle();
    }
    TEST_F(PresentPhaseWindowIntegrationTest, PresentDoesNotWaitForUnrelatedUnsignaledFrameFences) {
        make_surface();
        ASSERT_FALSE(HasFatalFailure());
        if (!physical._data->device.getSurfaceSupportKHR(queue_family, surface->_data->surface))
            GTEST_SKIP() << "Graphics queue cannot present";
        vsl::Swapchain chain(device, surface, std::nullopt, 64, 48);
        vsl::RenderPass pass(chain);
        vsl::FrameBuffer target(chain, pass);
        auto manager = make_manager();
        vsl::SynchroManager synchronization(device);
        auto fences = synchronization.createFence("frames", manager.getBuffer().getSize(), true);
        const auto nextSlot = (manager.getCurrentBufferIdx() + 1) % manager.getBuffer().getSize();
        const auto unrelated = (nextSlot + 1) % manager.getBuffer().getSize();
        fences.reset(static_cast<std::uint32_t>(unrelated));
        vsl::DefaultPhase phase(manager, chain, std::nullopt, std::nullopt, fences);
        EXPECT_EQ(phase.getCurrentIndex(), nextSlot);
        phase << vsl::command::RenderPassBegin(pass, target) << vsl::command::RenderPassEnd{};
        EXPECT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::Success);
        EXPECT_TRUE(phase.completion().wait());
        EXPECT_FALSE(fences.wait(static_cast<std::uint32_t>(unrelated), 0));
        device._data->device.waitIdle();
    }

}

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
    void invoke(vsl::DefaultPhaseStream &, vsl::CommandPool, vsl::CommandBuffer, vsl::CommandManager) override {}
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
        void invoke(vsl::DefaultPhaseStream &, vsl::CommandPool, vsl::CommandBuffer, vsl::CommandManager) {}
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
