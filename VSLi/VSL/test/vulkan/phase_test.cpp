#include "support/vulkan_fixture.hpp"
#include "support/window_fixture.hpp"
#include <VSL/vulkan/commands/image_readback.hpp>
namespace {
    struct RecordCommand : VSL_NAMESPACE::command::Command {
        std::shared_ptr<int> calls;
        explicit RecordCommand(std::shared_ptr<int> calls);
        void invoke(VSL_NAMESPACE::CommandPool, VSL_NAMESPACE::CommandBuffer, VSL_NAMESPACE::CommandManager) override;
    };

    struct RecordDelegate : VSL_NAMESPACE::command::DelegateCommand {
        std::shared_ptr<int> calls;
        explicit RecordDelegate(std::shared_ptr<int> calls);
        void invoke(VSL_NAMESPACE::DefaultPhaseStream& stream, VSL_NAMESPACE::CommandPool, VSL_NAMESPACE::CommandBuffer,
                    VSL_NAMESPACE::CommandManager manager) override;
    };

    using PhaseVulkanIntegrationTest = vsl_test::VulkanIntegrationTest;

    class PhaseCompletionVulkanIntegrationTest : public vsl_test::VulkanIntegrationTest {
        bool validate_vulkan() const override;
    };

    class PresentPhaseWindowIntegrationTest : public vsl_test::WindowIntegrationTest {
        bool validate_vulkan() const override;
    };

    struct VertexCountSource {
        std::size_t count;
        std::size_t get_vertex_size() { return count; }
        void invoke(VSL_NAMESPACE::CommandPool, VSL_NAMESPACE::CommandBuffer, VSL_NAMESPACE::CommandManager) {}
    };

    struct VertexCountConsumer {
        std::shared_ptr<std::optional<std::size_t>> received;
        void set_vertex_size(std::optional<std::size_t> value) { *received = value; }
        void invoke(VSL_NAMESPACE::CommandPool, VSL_NAMESPACE::CommandBuffer, VSL_NAMESPACE::CommandManager) {}
    };

    struct VertexCountDelegate : VSL_NAMESPACE::command::DelegateCommand, VSL_NAMESPACE::command::VertexSizeRequire {
        std::optional<std::size_t> received;
        void set_vertex_size(std::optional<std::size_t> value) override { received = value; }
        void invoke(VSL_NAMESPACE::DefaultPhaseStream&, VSL_NAMESPACE::CommandPool, VSL_NAMESPACE::CommandBuffer, VSL_NAMESPACE::CommandManager) override {}
    };

    struct IncrementVertexCount : VSL_NAMESPACE::command::Command,
                                  VSL_NAMESPACE::command::VertexSizeRequire,
                                  VSL_NAMESPACE::command::VertexSizeHolder {
        std::optional<std::size_t> input;
        void set_vertex_size(std::optional<std::size_t> count) override { input = count; }
        std::size_t get_vertex_size() override { return input.value_or(0) + 1; }
        void invoke(VSL_NAMESPACE::CommandPool, VSL_NAMESPACE::CommandBuffer, VSL_NAMESPACE::CommandManager) override {}
    };

    RecordCommand::RecordCommand(std::shared_ptr<int> calls) : calls(std::move(calls)) {}

    void RecordCommand::invoke(VSL_NAMESPACE::CommandPool, VSL_NAMESPACE::CommandBuffer, VSL_NAMESPACE::CommandManager) {
        ++*calls;
    }

    RecordDelegate::RecordDelegate(std::shared_ptr<int> calls) : calls(std::move(calls)) {}

    void RecordDelegate::invoke(VSL_NAMESPACE::DefaultPhaseStream& stream, VSL_NAMESPACE::CommandPool, VSL_NAMESPACE::CommandBuffer,
                                VSL_NAMESPACE::CommandManager manager) {
        EXPECT_EQ(stream.manager.data_, manager.data_);
        ++*calls;
    }

    TEST_F(PhaseVulkanIntegrationTest, PostponedSetupAndSubmissionFollowStateTransitions) {
        auto manager = make_manager();
        VSL_NAMESPACE::ComputePhase phase(manager, std::nullopt, std::nullopt, std::nullopt, true);
        EXPECT_EQ(phase.state, VSL_NAMESPACE::ComputePhase::State::BEFORE_SETUP);
        EXPECT_EQ(phase.submit(), VSL_NAMESPACE::ComputePhase::SubmitResult::FAILED);
        phase.setup();
        EXPECT_EQ(phase.state, VSL_NAMESPACE::ComputePhase::State::WAIT_SUBMIT);
        phase.setup();
        EXPECT_EQ(phase.state, VSL_NAMESPACE::ComputePhase::State::WAIT_SUBMIT);
        EXPECT_EQ(phase.submit(), VSL_NAMESPACE::ComputePhase::SubmitResult::SUCCESS);
        EXPECT_EQ(phase.state, VSL_NAMESPACE::ComputePhase::State::FINISH_SUBMIT);
        EXPECT_EQ(phase.submit(), VSL_NAMESPACE::ComputePhase::SubmitResult::FAILED);
        device.data_->device.waitIdle();
    }
    TEST_F(PhaseVulkanIntegrationTest, StreamsInvokeValueAndSharedCommandsAndDelegates) {
        auto calls = std::make_shared<int>(0);
        auto manager = make_manager();
        {
            auto phase = manager.start_phase<VSL_NAMESPACE::ComputePhase>();
            auto stream = phase << RecordCommand(calls);
            EXPECT_EQ(*calls, 1);
            stream << RecordCommand(calls) << RecordDelegate(calls);
            EXPECT_EQ(*calls, 3);
            stream << std::shared_ptr<VSL_NAMESPACE::command::Command>(std::make_shared<RecordCommand>(calls));
            stream << std::shared_ptr<VSL_NAMESPACE::command::DelegateCommand>(std::make_shared<RecordDelegate>(calls));
            EXPECT_EQ(*calls, 5);
            phase << std::shared_ptr<VSL_NAMESPACE::command::Command>(std::make_shared<RecordCommand>(calls));
            phase << std::shared_ptr<VSL_NAMESPACE::command::DelegateCommand>(std::make_shared<RecordDelegate>(calls));
            phase << RecordDelegate(calls);
            EXPECT_EQ(*calls, 8);
        }
        device.data_->device.waitIdle();
    }
    TEST_F(PhaseVulkanIntegrationTest, PostponedDefaultPhaseCanBeInspectedWithoutAcquiringAnImage) {
        auto manager = make_manager();
        VSL_NAMESPACE::DefaultPhase phase(manager, VSL_NAMESPACE::SwapchainAccessor{}, std::nullopt, std::nullopt, std::nullopt,
                                          true);
        EXPECT_EQ(phase.state, VSL_NAMESPACE::ComputePhase::State::BEFORE_SETUP);
        EXPECT_EQ(phase.get_current_index(), manager.get_current_buffer_idx());
        EXPECT_EQ(phase.submit(), VSL_NAMESPACE::ComputePhase::SubmitResult::FAILED);
        EXPECT_EQ(phase.get_image_index(), UINT32_MAX);
    }
} // namespace

namespace {

    bool PhaseCompletionVulkanIntegrationTest::validate_vulkan() const {
        return true;
    }

    TEST_F(PhaseCompletionVulkanIntegrationTest, RepeatedSubmissionsReuseBuffersWithoutExternalFenceOrIdle) {
        auto manager = make_manager();
        VSL_NAMESPACE::Completion previous;
        for (unsigned i = 0; i < 20; ++i) {
            VSL_NAMESPACE::ComputePhase phase(manager);
            if (previous)
                EXPECT_TRUE(previous.ready());
            auto token = phase.completion();
            EXPECT_EQ(token.status(), VSL_NAMESPACE::Completion::Status::PENDING);
            EXPECT_FALSE(token.wait(0));
            EXPECT_EQ(phase.submit(), VSL_NAMESPACE::ComputePhase::SubmitResult::SUCCESS);
            previous = token;
        }
        ASSERT_TRUE(previous.wait());
        EXPECT_EQ(previous.status(), VSL_NAMESPACE::Completion::Status::COMPLETE);
    }
    TEST_F(PhaseCompletionVulkanIntegrationTest, CancellationAndNestedRecordingDoNotSubmitOrResetActiveBuffer) {
        auto manager = make_manager();
        VSL_NAMESPACE::ComputePhase first(manager);
        EXPECT_THROW((VSL_NAMESPACE::ComputePhase(manager)), std::logic_error);
        auto cancelled = first.completion();
        first.cancel();
        EXPECT_EQ(cancelled.status(), VSL_NAMESPACE::Completion::Status::CANCELLED);
        EXPECT_FALSE(cancelled.wait(0));
        EXPECT_EQ(first.submit(), VSL_NAMESPACE::ComputePhase::SubmitResult::FAILED);
        VSL_NAMESPACE::ComputePhase next(manager);
        EXPECT_EQ(next.submit(), VSL_NAMESPACE::ComputePhase::SubmitResult::SUCCESS);
        EXPECT_TRUE(next.completion().wait());
    }
    TEST_F(PhaseCompletionVulkanIntegrationTest, OldCompletionRemainsCompleteWhenNativeFenceIsReused) {
        auto manager = make_manager();
        VSL_NAMESPACE::ComputePhase first(manager);
        auto original = first.completion();
        first.submit();
        VSL_NAMESPACE::ComputePhase second(manager);
        EXPECT_TRUE(original.ready());
        second.submit();
        EXPECT_TRUE(original.wait(0));
        EXPECT_TRUE(second.completion().wait());
    }
    TEST_F(PhaseCompletionVulkanIntegrationTest, SubmittedWorkRetainsItsWaitAndSignalSemaphores) {
        auto manager = make_manager();
        // 寿命の検証に失敗しても使用中のハンドルを破棄しないよう、別の参照を保持する。
        std::shared_ptr<VSL_NAMESPACE::impl::SemaphoreImpl> available_owner, finished_owner;
        VSL_NAMESPACE::Completion completion;
        {
            VSL_NAMESPACE::SynchroManager synchronization(device);
            auto available = synchronization.create_semaphore("available", manager.get_buffer().get_size());
            auto finished = synchronization.create_semaphore("finished", manager.get_buffer().get_size());
            available_owner = available.data_;
            finished_owner = finished.data_;
            auto signal = available.data_->semaphores.at(manager.get_current_buffer_idx());
            manager.data_->graphics_queue.submit(vk::SubmitInfo{
                .signalSemaphoreCount = 1, .pSignalSemaphores = &signal});
            VSL_NAMESPACE::ComputePhase phase(manager, available, finished);
            completion = phase.completion();
            ASSERT_EQ(phase.submit(), VSL_NAMESPACE::ComputePhase::SubmitResult::SUCCESS);
        }
        EXPECT_GT(available_owner.use_count(), 1);
        EXPECT_GT(finished_owner.use_count(), 1);
        ASSERT_TRUE(completion.wait());
        manager.wait_idle();
    }
    TEST_F(PhaseCompletionVulkanIntegrationTest, ExternalFenceWaitAndResetPreserveTheOriginalCompletion) {
        for (unsigned mode = 0; mode < 3; ++mode) {
            SCOPED_TRACE(mode);
            auto manager = make_manager();
            VSL_NAMESPACE::SynchroManager synchronization(device);
            auto fence = synchronization.create_fence("frames", manager.get_buffer().get_size(), true);
            VSL_NAMESPACE::ComputePhase first(manager, std::nullopt, std::nullopt, fence);
            auto original = first.completion();
            ASSERT_EQ(first.submit(), VSL_NAMESPACE::ComputePhase::SubmitResult::SUCCESS);
            if (mode == 0) {
                ASSERT_TRUE(fence.wait(first.frame_slot, UINT64_MAX));
                fence.reset(first.frame_slot);
            } else if (mode == 1) {
                ASSERT_TRUE(fence.wait());
                fence.reset();
            } else {
                // 別のAPIで完了を待ってから公開APIでリセットする場合も確かめる。
                ASSERT_EQ(device.data_->device.waitForFences(fence.data_->fences, true, UINT64_MAX), vk::Result::eSuccess);
                fence.reset(first.frame_slot);
            }
            EXPECT_TRUE(original.ready());
            EXPECT_TRUE(original.wait(0));
            // 回帰時にも破棄時のフェンス待ちが停止しないよう、再度シグナルを送る。
            manager.data_->graphics_queue.submit(vk::SubmitInfo{}, fence.data_->fences.at(first.frame_slot));
            ASSERT_TRUE(fence.wait(first.frame_slot, UINT64_MAX));
            fence.destroy();
            EXPECT_TRUE(original.ready());
        }
    }

    TEST_F(PhaseCompletionVulkanIntegrationTest, RetainedResourcesSurviveUntilCompletionTokenIsReleased) {
        auto manager = make_manager();
        std::weak_ptr<int> resource;
        {
            VSL_NAMESPACE::ComputePhase phase(manager);
            auto retained = std::make_shared<int>(7);
            resource = retained;
            phase.completion().retain(retained);
            retained.reset();
            EXPECT_FALSE(resource.expired());
            phase.submit();
            EXPECT_TRUE(phase.completion().wait());
        }
        {
            VSL_NAMESPACE::ComputePhase next(manager);
            next.cancel();
        }
        // 同じスロットへ再送信するまで、最後の送信がリソースを保持する。
        {
            VSL_NAMESPACE::ComputePhase next(manager);
            next.submit();
            next.completion().wait();
        }
        EXPECT_TRUE(resource.expired());
    }
} // namespace

namespace {

    bool PresentPhaseWindowIntegrationTest::validate_vulkan() const {
        return true;
    }

    TEST_F(PresentPhaseWindowIntegrationTest, UsesAcquiredImageForFramebufferReadbackAndPresentAcrossFrames) {
        make_surface();
        ASSERT_FALSE(HasFatalFailure());
        if (!physical.data_->device.getSurfaceSupportKHR(queue_family, surface->data_->surface))
            GTEST_SKIP() << "Graphics queue cannot present";
        if (!(physical.data_->device.getSurfaceCapabilitiesKHR(surface->data_->surface).supportedUsageFlags & vk::ImageUsageFlagBits::eTransferSrc))
            GTEST_SKIP() << "Surface images cannot be read back";
        VSL_NAMESPACE::Swapchain chain(device, surface, std::nullopt, 64, 48,
                                       VSL_NAMESPACE::ImageType::COLOR_ATTACHMENT | VSL_NAMESPACE::ImageType::TRANSFER_SRC);
        VSL_NAMESPACE::RenderPass pass(chain);
        VSL_NAMESPACE::FrameBuffer target(chain, pass);
        auto manager = make_manager();
        auto bytes = std::size_t(chain.data_->extent.width) * chain.data_->extent.height * 4;
        VSL_NAMESPACE::StagingBuffer<VSL_NAMESPACE::MemoryType::TRANSFER_DESTINATION> destination(device, bytes);
        bool different_index = false;
        for (unsigned frame = 0; frame < 12; ++frame) {
            VSL_NAMESPACE::DefaultPhase phase(manager, chain); // セマフォやフェンスを外から渡さない。
            auto index = phase.get_image_index();
            different_index |= index != phase.get_current_index();
            auto begin = VSL_NAMESPACE::command::RenderPassBegin(pass, target);
            begin.clear_values[0] = VSL_NAMESPACE::ClearColor{1.f, 0.f, 0.f, 1.f};
            phase << begin << VSL_NAMESPACE::command::RenderPassEnd{}
                  << VSL_NAMESPACE::command::ImageReadback(&destination, chain.images.at(index), VSL_NAMESPACE::ImageLayout::PRESENT);
            EXPECT_EQ(phase.submit_commands(), VSL_NAMESPACE::ComputePhase::SubmitResult::SUCCESS);
            EXPECT_EQ(phase.present(), VSL_NAMESPACE::ComputePhase::SubmitResult::SUCCESS);
            ASSERT_TRUE(phase.completion().wait());
            auto mapped = destination.data();
            const bool bgra = chain.get_format().type() == VSL_NAMESPACE::data_format::FormatTypes::SRGB8_BGRA ||
                              chain.get_format().type() == VSL_NAMESPACE::data_format::FormatTypes::UNSIGNED_NORMALIZED8_BGRA;
            const auto* pixel = static_cast<const unsigned char*>(mapped.data);
            EXPECT_EQ(pixel[bgra ? 2 : 0], 255);
            EXPECT_EQ(pixel[1], 0);
            EXPECT_EQ(pixel[3], 255);
            EXPECT_EQ(phase.present(), VSL_NAMESPACE::ComputePhase::SubmitResult::FAILED);
        }
        // 画像の取得順がフレームのスロット順と一致する実装でも動作する。
        (void)different_index;
        device.data_->device.waitIdle();
    }
    TEST_F(PresentPhaseWindowIntegrationTest, CancelledAcquiredFrameCanBeFollowedByAnotherFrame) {
        make_surface();
        ASSERT_FALSE(HasFatalFailure());
        if (!physical.data_->device.getSurfaceSupportKHR(queue_family, surface->data_->surface))
            GTEST_SKIP() << "Graphics queue cannot present";
        VSL_NAMESPACE::Swapchain chain(device, surface, std::nullopt, 64, 48);
        VSL_NAMESPACE::RenderPass pass(chain);
        VSL_NAMESPACE::FrameBuffer target(chain, pass);
        auto manager = make_manager();
        {
            VSL_NAMESPACE::DefaultPhase phase(manager, chain);
            auto discarded = phase.completion();
            phase << VSL_NAMESPACE::command::RenderPassBegin(pass, target); // 終了コマンドを省略し、記録途中のキャンセルを確かめる。
            phase.cancel();
            EXPECT_EQ(discarded.status(), VSL_NAMESPACE::Completion::Status::CANCELLED);
            EXPECT_TRUE(phase.completion().wait());
        }
        {
            VSL_NAMESPACE::DefaultPhase phase(manager, chain);
            phase << VSL_NAMESPACE::command::RenderPassBegin(pass, target) << VSL_NAMESPACE::command::RenderPassEnd{};
            EXPECT_EQ(phase.submit(), VSL_NAMESPACE::ComputePhase::SubmitResult::SUCCESS);
            EXPECT_TRUE(phase.completion().wait());
        }
        device.data_->device.waitIdle();
    }
    TEST_F(PresentPhaseWindowIntegrationTest, PresentDoesNotWaitForUnrelatedUnsignaledFrameFences) {
        make_surface();
        ASSERT_FALSE(HasFatalFailure());
        if (!physical.data_->device.getSurfaceSupportKHR(queue_family, surface->data_->surface))
            GTEST_SKIP() << "Graphics queue cannot present";
        VSL_NAMESPACE::Swapchain chain(device, surface, std::nullopt, 64, 48);
        VSL_NAMESPACE::RenderPass pass(chain);
        VSL_NAMESPACE::FrameBuffer target(chain, pass);
        auto manager = make_manager();
        VSL_NAMESPACE::SynchroManager synchronization(device);
        auto fences = synchronization.create_fence("frames", manager.get_buffer().get_size(), true);
        const auto next_slot = (manager.get_current_buffer_idx() + 1) % manager.get_buffer().get_size();
        const auto unrelated = (next_slot + 1) % manager.get_buffer().get_size();
        fences.reset(static_cast<std::uint32_t>(unrelated));
        VSL_NAMESPACE::DefaultPhase phase(manager, chain, std::nullopt, std::nullopt, fences);
        EXPECT_EQ(phase.get_current_index(), next_slot);
        phase << VSL_NAMESPACE::command::RenderPassBegin(pass, target) << VSL_NAMESPACE::command::RenderPassEnd{};
        EXPECT_EQ(phase.submit(), VSL_NAMESPACE::ComputePhase::SubmitResult::SUCCESS);
        EXPECT_TRUE(phase.completion().wait());
        EXPECT_FALSE(fences.wait(static_cast<std::uint32_t>(unrelated), 0));
        device.data_->device.waitIdle();
    }

} // namespace

namespace {
    TEST_F(PhaseVulkanIntegrationTest, ValueCommandsExportAndReceiveVertexCountsThroughTheirOwnRoles) {
        auto manager = make_manager();
        auto received = std::make_shared<std::optional<std::size_t>>(99);
        VSL_NAMESPACE::DefaultPhaseStream stream{manager};
        stream << VertexCountConsumer{received};
        EXPECT_FALSE(*received);
        stream << VertexCountSource{17} << VertexCountConsumer{received};
        EXPECT_EQ(stream.vertex_size, 17u);
        EXPECT_EQ(*received, 17u);
        stream << VertexCountSource{0} << VertexCountConsumer{received};
        EXPECT_EQ(*received, 0u);
    }
    TEST_F(PhaseVulkanIntegrationTest, ValueAndSharedDelegatesReceiveTheSameVertexCount) {
        auto manager = make_manager();
        VSL_NAMESPACE::DefaultPhaseStream stream{manager, 23};
        struct ValueDelegate {
            std::shared_ptr<std::optional<std::size_t>> received;
            void set_vertex_size(std::optional<std::size_t> value) { *received = value; }
            void invoke(VSL_NAMESPACE::DefaultPhaseStream&, VSL_NAMESPACE::CommandPool, VSL_NAMESPACE::CommandBuffer, VSL_NAMESPACE::CommandManager) {}
        };
        auto received = std::make_shared<std::optional<std::size_t>>();
        stream << ValueDelegate{received};
        EXPECT_EQ(*received, 23u);
        auto shared = std::make_shared<VertexCountDelegate>();
        stream << std::shared_ptr<VSL_NAMESPACE::command::DelegateCommand>(shared);
        EXPECT_EQ(shared->received, *received);
    }
} // namespace

namespace {
    TEST_F(PhaseVulkanIntegrationTest, CommandsConsumeThePreviousCountBeforeExportingTheirOwnCount) {
        auto manager = make_manager();
        VSL_NAMESPACE::DefaultPhaseStream value_stream{manager, 17};
        value_stream << IncrementVertexCount{};
        EXPECT_EQ(value_stream.vertex_size, 18u);
        auto command = std::make_shared<IncrementVertexCount>();
        VSL_NAMESPACE::DefaultPhaseStream shared_stream{manager, 17};
        shared_stream << std::shared_ptr<VSL_NAMESPACE::command::Command>(command);
        EXPECT_EQ(command->input, 17u);
        EXPECT_EQ(shared_stream.vertex_size, 18u);
        EXPECT_EQ(value_stream.vertex_size, shared_stream.vertex_size);
    }
} // namespace
