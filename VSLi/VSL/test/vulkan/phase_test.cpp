#include <VSL/lifetime/command_domain.hpp>
#include <VSL/lifetime/presentation_retention.hpp>
#include "support/deferred_action.hpp"
#include "support/vulkan_fixture.hpp"
#include "support/window_fixture.hpp"
#include <VSL/vulkan/commands/image_readback.hpp>
#include <VSL/lifetime/resource_arena.hpp>
#include <VSL/operation/presentation_ops.hpp>
#include <VSL/operation/synchronization_ops.hpp>
#include <VSL/sync/presentation_state.hpp>
#include <functional>
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
    class AcquireFencePoolWindowIntegrationTest : public vsl_test::WindowIntegrationTest {
        bool validate_vulkan() const override { return true; }
        void SetUp() override;
    };
    struct AcquireProbe {
        unsigned calls = 0;
        VkFence fence = VK_NULL_HANDLE;
        VkResult failure = VK_SUCCESS;
        std::function<void()> after_native;
        static VkResult Acquire(void* context, VkDevice device, VkSwapchainKHR chain, std::uint64_t timeout,
                                VkSemaphore available, VkFence fence, std::uint32_t* image) noexcept;
        vsl::operation::PresentationNativeCalls native_calls() { return {.context = this, .acquire = Acquire}; }
    };
    struct ResetAcquireProbe {
        unsigned calls = 0;
        VkResult failure = VK_ERROR_OUT_OF_HOST_MEMORY;
        static VkResult Reset(void* context, VkDevice device, std::uint32_t count, const VkFence* fences) noexcept;
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
    TEST_F(PhaseCompletionVulkanIntegrationTest, TerminalWorkReleasesSynchronizationOwnersBeforeItsSlotIsReplaced) {
        for (bool cancel : {false, true}) {
            SCOPED_TRACE(cancel);
            auto commands = make_manager().independent(1);
            std::weak_ptr<vsl::impl::SemaphoreImpl> signal;
            std::weak_ptr<vsl::impl::FenceImpl> fence;
            std::unique_ptr<vsl::ComputePhase> phase;
            {
                vsl::SynchroManager synchronization(device);
                auto finished = synchronization.create_semaphore("terminal signal");
                auto external = synchronization.create_fence("terminal fence", false);
                signal = finished.data_;
                fence = external.data_;
                phase = std::make_unique<vsl::ComputePhase>(commands, std::nullopt, finished, external);
            }
            const auto result = phase->completion();
            const auto work = result.data_->work.lock();
            ASSERT_TRUE(work);
            if (!cancel)
                ASSERT_EQ(phase->submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
            phase->calculation_finish.reset();
            phase->in_flight_fence.reset();
            EXPECT_FALSE(signal.expired());
            EXPECT_FALSE(fence.expired());
            if (cancel)
                phase->cancel();
            else
                ASSERT_TRUE(result.wait());
            EXPECT_EQ(result.status(), cancel ? vsl::Completion::Status::CANCELLED : vsl::Completion::Status::COMPLETE);
            EXPECT_TRUE(signal.expired());
            EXPECT_TRUE(fence.expired());
            EXPECT_EQ(work->signal_semaphore, nullptr);
            EXPECT_EQ(work->external_fence, nullptr);
            EXPECT_TRUE(work->completion_resources.empty());
            EXPECT_TRUE(std::ranges::all_of(work->synchronization_owners, [](size_t index) {
                return index == vsl::impl::SubmissionWork::NO_OWNER;
            }));
        }
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

    TEST_F(PhaseCompletionVulkanIntegrationTest, LegacyResourcesBelongToTheSubmittedWorkUntilSlotReplacement) {
        auto manager = make_manager();
        std::weak_ptr<int> resource;
        VSL_NAMESPACE::Completion result;
        std::weak_ptr<VSL_NAMESPACE::impl::SubmissionWork> work;
        {
            VSL_NAMESPACE::ComputePhase phase(manager);
            result = phase.completion();
            work = result.data_->work;
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
        EXPECT_TRUE(work.expired());
        EXPECT_TRUE(result.ready());
    }
    TEST_F(PhaseCompletionVulkanIntegrationTest, SavedResultDoesNotKeepNativeWorkAndFenceAliveAfterManagerDestruction) {
        vsl::Completion result;
        vsl::CompletionObservation observation;
        std::weak_ptr<vsl::impl::SubmissionWork> work;
        std::weak_ptr<vk::UniqueFence> fence;
        std::weak_ptr<int> owner;
        {
            auto manager = make_manager();
            auto phase = manager.start_phase<vsl::ComputePhase>();
            result = phase.completion();
            observation = vsl::CompletionObservation(result);
            work = result.data_->work;
            fence = work.lock()->owned_fence;
            auto retained = std::make_shared<int>(7);
            owner = retained;
            result.retain(retained);
            ASSERT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
        }
        EXPECT_TRUE(work.expired());
        EXPECT_TRUE(fence.expired());
        EXPECT_TRUE(owner.expired());
        EXPECT_TRUE(result.ready());
        EXPECT_EQ(observation.status(), vsl::Completion::Status::COMPLETE);
    }
    TEST_F(PhaseCompletionVulkanIntegrationTest, SubmittedWorkKeepsTheOwnerWithoutASavedCompletionUntilObservedIdle) {
        auto manager = make_manager();
        vsl::ResourceUseState use;
        std::weak_ptr<int> owner;
        {
            auto phase = manager.start_phase<vsl::ComputePhase>();
            auto retained = std::make_shared<int>(7);
            owner = retained;
            manager.get_buffer().retain_until_complete(retained);
            use.mark_used(phase.completion());
            ASSERT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
        }
        EXPECT_FALSE(owner.expired());
        manager.wait_idle();
        EXPECT_TRUE(owner.expired());
        EXPECT_FALSE(use.busy());
    }
    TEST_F(PhaseCompletionVulkanIntegrationTest, TerminalReleaseMayResetTheObservingCompletionHandle) {
        auto manager = make_manager();
        auto phase = manager.start_phase<vsl::ComputePhase>();
        vsl::Completion result = phase.completion();
        size_t releases = 0;
        {
            auto owner = std::shared_ptr<void>(new int(7), [&](void* value) {
                result = {};
                ++releases;
                delete static_cast<int*>(value);
            });
            manager.get_buffer().retain_until_complete(owner);
        }
        ASSERT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
        manager.get_device().data_->device.waitIdle();
        EXPECT_EQ(result.status(), vsl::Completion::Status::COMPLETE);
        EXPECT_FALSE(result);
        EXPECT_EQ(releases, 1u);
        EXPECT_TRUE(phase.completion().ready());
    }
    TEST_F(PhaseCompletionVulkanIntegrationTest, IdleReleaseCanSubmitAnotherJobWithoutDestroyingTheWorkBeingReleased) {
        auto manager = make_manager();
        vsl::Completion first, next;
        std::weak_ptr<vsl::impl::SubmissionWork> old_work;
        {
            auto phase = manager.start_phase<vsl::ComputePhase>();
            first = phase.completion();
            old_work = first.data_->work;
            auto owner = std::shared_ptr<void>(new int(7), [&](void* value) {
                delete static_cast<int*>(value);
                auto followup = manager.start_phase<vsl::ComputePhase>();
                next = followup.completion();
                EXPECT_EQ(followup.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
            });
            manager.get_buffer().retain_until_complete(owner);
            ASSERT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
        }
        manager.wait_idle();
        EXPECT_TRUE(old_work.expired());
        EXPECT_TRUE(first.ready());
        ASSERT_TRUE(next);
        EXPECT_TRUE(next.wait());
    }
    TEST_F(PhaseCompletionVulkanIntegrationTest, PreparationDoesNotResetAJobStartedByAReleaseCallback) {
        for (const bool submit_peer : {false, true}) {
            auto manager = make_manager();
            std::unique_ptr<vsl::ComputePhase> peer;
            {
                auto first = manager.start_phase<vsl::ComputePhase>();
                auto owner = std::shared_ptr<void>(new int(7), [&](void* value) {
                    delete static_cast<int*>(value);
                    peer = std::make_unique<vsl::ComputePhase>(manager);
                    if (submit_peer)
                        EXPECT_EQ(peer->submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
                });
                manager.get_buffer().retain_until_complete(owner);
                ASSERT_EQ(first.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
            }
            EXPECT_THROW(manager.start_phase<vsl::ComputePhase>(), std::logic_error);
            ASSERT_TRUE(peer);
            if (!submit_peer) {
                EXPECT_EQ(peer->completion().status(), vsl::Completion::Status::PENDING);
                ASSERT_EQ(peer->submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
            }
            ASSERT_TRUE(peer->completion().wait());
        }
    }
    TEST_F(PhaseCompletionVulkanIntegrationTest, PreparationCanReleaseTheCurrentPostponedPhase) {
        for (const bool through_shared_fence : {false, true}) {
            SCOPED_TRACE(through_shared_fence);
            auto manager = make_manager().independent(1);
            auto source = through_shared_fence ? make_manager().independent(1) : manager;
            vsl::SynchroManager sync(device);
            const auto fence = sync.create_fence("completion");
            std::unique_ptr<vsl::ComputePhase> pending;
            bool invoked = false;
            const vsl_test::DeferredAction callback([&] {
                invoked = true;
                pending.reset();
            });
            {
                auto previous = source.start_phase<vsl::ComputePhase>(std::nullopt, std::nullopt, fence);
                source.get_buffer().retain_until_complete(callback.owner());
                ASSERT_EQ(previous.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
            }
            pending = std::make_unique<vsl::ComputePhase>(manager, std::nullopt, std::nullopt, fence, true);
            EXPECT_NO_THROW(pending->setup());
            EXPECT_TRUE(invoked);
            EXPECT_FALSE(pending);
            auto next = manager.start_phase<vsl::ComputePhase>(std::nullopt, std::nullopt, fence);
            EXPECT_EQ(next.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
            EXPECT_TRUE(next.completion().wait());
        }
    }
    TEST_F(PhaseCompletionVulkanIntegrationTest, SubmitPreflightFailurePublishesFailureAndReleasesCompletionPinsOnce) {
        auto manager = make_manager();
        vsl::SynchroManager sync(device);
        auto available = sync.create_semaphore("unused");
        vsl::ComputePhase phase(manager, available, std::nullopt, std::nullopt, false, static_cast<vsl::PipelineStage>(0));
        const auto result = phase.completion();
        size_t releases = 0;
        {
            auto owner = std::shared_ptr<void>(new int(7), [&](void* value) { ++releases; delete static_cast<int*>(value); });
            manager.get_buffer().retain_until_complete(owner);
        }
        EXPECT_THROW(phase.submit(), std::invalid_argument);
        EXPECT_EQ(result.status(), vsl::Completion::Status::FAILED);
        EXPECT_EQ(result.failure(), vsl::Completion::Failure::SUBMIT_FAILED);
        EXPECT_EQ(releases, 1u);
        EXPECT_FALSE(manager.get_buffer().data_->recording_context->recording);
        phase.cancel();
        EXPECT_EQ(releases, 1u);
        auto next = manager.start_phase<vsl::ComputePhase>();
        ASSERT_EQ(next.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
        ASSERT_TRUE(next.completion().wait());
    }
} // namespace

namespace {

    bool PresentPhaseWindowIntegrationTest::validate_vulkan() const {
        return true;
    }

    void AcquireFencePoolWindowIntegrationTest::SetUp() {
        vsl_test::WindowIntegrationTest::SetUp();
        if (HasFatalFailure() || IsSkipped())
            return;
        make_surface();
        if (HasFatalFailure())
            return;
        if (!physical.data_->device.getSurfaceSupportKHR(queue_family, surface->data_->surface))
            GTEST_SKIP() << "Graphics queue cannot present";
    }
    VkResult AcquireProbe::Acquire(void* context, VkDevice device, VkSwapchainKHR chain, std::uint64_t timeout,
                                   VkSemaphore available, VkFence fence, std::uint32_t* image) noexcept {
        auto& probe = *static_cast<AcquireProbe*>(context);
        ++probe.calls;
        probe.fence = fence;
        VkResult result;
        if (probe.failure == VK_SUCCESS)
            result = vkAcquireNextImageKHR(device, chain, timeout, available, fence, image);
        else {
            *image = UINT32_MAX - 1;
            result = probe.failure;
        }
        if (auto callback = std::move(probe.after_native))
            callback();
        return result;
    }
    VkResult ResetAcquireProbe::Reset(void* context, VkDevice device, std::uint32_t count, const VkFence* fences) noexcept {
        auto& probe = *static_cast<ResetAcquireProbe*>(context);
        ++probe.calls;
        return probe.failure == VK_SUCCESS ? vkResetFences(device, count, fences) : probe.failure;
    }
    TEST_F(AcquireFencePoolWindowIntegrationTest, PoolIsLazyAndReusesOnlyTheFrameElementsWithoutReobservingOldProofs) {
        auto manager = make_manager().independent(3);
        const auto domain = vsl::lifetime::CommandAccess::Owner(manager.get_buffer());
        EXPECT_FALSE(domain->acquire_fences);
        {
            auto compute = manager.start_phase<vsl::ComputePhase>();
            compute.cancel();
        }
        EXPECT_FALSE(domain->acquire_fences);
        vsl::Swapchain chain(device, surface, {}, 64, 48);
        AcquireProbe probe;
        const vsl::operation::ScopedPresentationNativeCalls native(probe.native_calls());
        std::shared_ptr<vsl::impl::FenceImpl> source;
        std::vector<vk::Fence> handles;
        std::shared_ptr<vsl::sync::FenceProof> proof;
        for (unsigned frame = 0; frame < 12; ++frame) {
            auto phase = manager.start_phase<vsl::DefaultPhase>(chain);
            ASSERT_TRUE(domain->acquire_fences);
            if (!source) {
                source = domain->acquire_fences;
                handles = source->fences;
            }
            EXPECT_EQ(domain->acquire_fences, source);
            ASSERT_EQ(source->fences.size(), 3u);
            EXPECT_EQ(source->fences, handles);
            EXPECT_EQ(probe.fence, static_cast<VkFence>(handles.at(phase.frame_slot)));
            const auto current = std::get<std::weak_ptr<vsl::sync::FenceProof>>(source->uses.at(phase.frame_slot).use).lock();
            ASSERT_TRUE(current);
            if (!proof)
                proof = current;
            phase.cancel();
            ASSERT_TRUE(phase.completion().wait());
        }
        EXPECT_EQ(probe.calls, 12u);
        ASSERT_TRUE(proof);
        vsl::FenceHolder{source}.reset();
        EXPECT_EQ(device.data_->device.getFenceStatus(proof->fence), vk::Result::eNotReady);
        EXPECT_TRUE(vsl::sync::poll_fence_proof(*proof));
        EXPECT_EQ(proof->status, vsl::sync::FenceProofStatus::COMPLETE);
        manager.wait_idle();
    }
    TEST_F(AcquireFencePoolWindowIntegrationTest, FailedAcquisitionRejectsItsOutputAndReleasesTheReservationForReuse) {
        auto manager = make_manager().independent(1);
        const auto domain = vsl::lifetime::CommandAccess::Owner(manager.get_buffer());
        vsl::Swapchain chain(device, surface, {}, 64, 48);
        vsl::SynchroManager synchronization(device);
        auto source = synchronization.create_fence("acquisition", false);
        AcquireProbe probe;
        const vsl::operation::ScopedPresentationNativeCalls native(probe.native_calls());
        for (auto error : {VK_ERROR_OUT_OF_DATE_KHR, VK_TIMEOUT, VK_ERROR_OUT_OF_HOST_MEMORY, VK_ERROR_OUT_OF_DEVICE_MEMORY}) {
            SCOPED_TRACE(error);
            probe.failure = error;
            vsl::DefaultPhase phase(manager, chain, std::nullopt, std::nullopt, std::nullopt, true, source);
            if (error == VK_ERROR_OUT_OF_DATE_KHR)
                EXPECT_NO_THROW(phase.setup());
            else
                EXPECT_THROW(phase.setup(), std::runtime_error);
            EXPECT_EQ(phase.get_image_index(), UINT32_MAX);
            EXPECT_FALSE(domain->context.acquired_image_index);
            const auto registry = vsl::lifetime::presentation_domain(chain.data_);
            ASSERT_FALSE(registry->acquisitions.empty());
            const auto& evidence = registry->acquisitions.back();
            EXPECT_EQ(evidence->image, UINT32_MAX);
            EXPECT_EQ(evidence->proof.status, vsl::sync::FenceProofStatus::NOT_ACQUIRED);
            EXPECT_TRUE(std::holds_alternative<std::monostate>(source.data_->uses[0].use));
            EXPECT_FALSE(domain->acquire_fences);
            EXPECT_NO_THROW(source.reset());
        }
        probe.failure = VK_SUCCESS;
        auto next = manager.start_phase<vsl::DefaultPhase>(chain, std::nullopt, std::nullopt, std::nullopt, false, source);
        EXPECT_EQ(probe.fence, static_cast<VkFence>(source.data_->fences[0]));
        next.cancel();
        EXPECT_TRUE(next.completion().wait());
        manager.wait_idle();
    }
    TEST_F(AcquireFencePoolWindowIntegrationTest, AnOlderPresentationCannotReleaseTheReusedFenceReservation) {
        auto manager = make_manager().independent(1);
        const auto domain = vsl::lifetime::CommandAccess::Owner(manager.get_buffer());
        vsl::Swapchain chain(device, surface, {}, 64, 48);
        auto prior = std::make_unique<vsl::DefaultPhase>(manager, chain);
        prior->cancel();
        ASSERT_TRUE(prior->completion().wait());
        const auto source = domain->acquire_fences;
        const auto old = std::get<std::weak_ptr<vsl::sync::FenceProof>>(source->uses[0].use).lock();
        ASSERT_TRUE(old);
        auto next = manager.start_phase<vsl::DefaultPhase>(chain);
        const auto current = std::get<std::weak_ptr<vsl::sync::FenceProof>>(source->uses[0].use).lock();
        ASSERT_TRUE(current);
        EXPECT_NE(current, old);
        prior.reset();
        ASSERT_TRUE(std::holds_alternative<std::weak_ptr<vsl::sync::FenceProof>>(source->uses[0].use));
        EXPECT_EQ(std::get<std::weak_ptr<vsl::sync::FenceProof>>(source->uses[0].use).lock(), current);
        EXPECT_EQ(old->status, vsl::sync::FenceProofStatus::COMPLETE);
        next.cancel();
        EXPECT_TRUE(next.completion().wait());
        manager.wait_idle();
    }
    TEST_F(AcquireFencePoolWindowIntegrationTest, FailedPoolResetRestoresTheOldProofAndAllowsTheNextReservation) {
        auto manager = make_manager().independent(1);
        const auto domain = vsl::lifetime::CommandAccess::Owner(manager.get_buffer());
        vsl::Swapchain chain(device, surface, {}, 64, 48);
        auto prior = std::make_unique<vsl::DefaultPhase>(manager, chain);
        prior->cancel();
        ASSERT_TRUE(prior->completion().wait());
        const auto source = domain->acquire_fences;
        const auto old = std::get<std::weak_ptr<vsl::sync::FenceProof>>(source->uses[0].use).lock();
        ASSERT_TRUE(old);
        AcquireProbe acquisition;
        const vsl::operation::ScopedPresentationNativeCalls native(acquisition.native_calls());
        ResetAcquireProbe reset;
        const vsl::operation::ScopedSynchronizationNativeCalls resetting({.context = &reset, .reset_fences = ResetAcquireProbe::Reset});
        EXPECT_THROW((vsl::DefaultPhase(manager, chain)), vk::OutOfHostMemoryError);
        EXPECT_EQ(reset.calls, 1u);
        EXPECT_EQ(acquisition.calls, 0u);
        EXPECT_EQ(std::get<std::weak_ptr<vsl::sync::FenceProof>>(source->uses[0].use).lock(), old);
        EXPECT_EQ(old->status, vsl::sync::FenceProofStatus::COMPLETE);
        reset.failure = VK_SUCCESS;
        auto next = manager.start_phase<vsl::DefaultPhase>(chain);
        EXPECT_EQ(reset.calls, 2u);
        EXPECT_EQ(acquisition.calls, 1u);
        next.cancel();
        EXPECT_TRUE(next.completion().wait());
        manager.wait_idle();
    }
    TEST_F(AcquireFencePoolWindowIntegrationTest, ExternalSourcesAreValidatedBeforeAcquisitionOrFenceReset) {
        auto manager = make_manager().independent(2);
        vsl::Swapchain chain(device, surface, {}, 64, 48);
        vsl::SynchroManager synchronization(device);
        auto small = synchronization.create_fence("small", false);
        auto same = synchronization.create_fence("graphics", std::size_t{2}, true);
        auto foreign_device = vsl::LogicalDeviceAccessor::Create(physical);
        vsl::SynchroManager foreign(foreign_device);
        auto wrong = foreign.create_fence("foreign", std::size_t{2}, false);
        AcquireProbe probe;
        const vsl::operation::ScopedPresentationNativeCalls native(probe.native_calls());
        for (const auto& source : {vsl::FenceHolder{}, small, wrong})
            EXPECT_THROW((vsl::DefaultPhase(manager, chain, std::nullopt, std::nullopt, std::nullopt, false, source)), std::invalid_argument);
        EXPECT_THROW((vsl::DefaultPhase(manager, chain, std::nullopt, std::nullopt, same, false, same)), std::invalid_argument);
        EXPECT_EQ(probe.calls, 0u);
        EXPECT_TRUE(same.wait(std::uint64_t{0}));
        EXPECT_FALSE(vsl::lifetime::CommandAccess::Owner(manager.get_buffer())->acquire_fences);
    }
    TEST_F(AcquireFencePoolWindowIntegrationTest, DistinctFenceGroupsShareAnArenaOwnerAndOutliveTheirOriginalWrappers) {
        auto manager = make_manager().independent(1);
        vsl::Swapchain chain(device, surface, {}, 64, 48);
        std::weak_ptr<void> owner;
        std::unique_ptr<vsl::DefaultPhase> phase;
        {
            static constexpr std::array<vsl::typed::ResourceRequest, 2> REQUESTS{{{{1}, vsl::storage::ResourceKind::FENCE}, {{2}, vsl::storage::ResourceKind::FENCE}}};
            static constexpr vsl::typed::ResourcePlan<2> PLAN(REQUESTS);
            const std::array<vsl::lifetime::ResourceInit, 2> inputs{vsl::FenceCreateInfo{1, false}, vsl::FenceCreateInfo{1, false}};
            vsl::lifetime::ResourceArena arena(device, PLAN, inputs);
            owner = arena.owner();
            const auto graphics = arena.fence({1}), acquisition = arena.fence({2});
            EXPECT_FALSE(graphics.data_.owner_before(acquisition.data_));
            EXPECT_FALSE(acquisition.data_.owner_before(graphics.data_));
            EXPECT_NE(graphics.data_->fences[0], acquisition.data_->fences[0]);
            phase = std::make_unique<vsl::DefaultPhase>(manager, chain, std::nullopt, std::nullopt, graphics, false, acquisition);
        }
        EXPECT_FALSE(owner.expired());
        phase->cancel();
        ASSERT_TRUE(phase->completion().wait());
        phase.reset();
        EXPECT_TRUE(owner.expired());
        manager.wait_idle();
    }
    TEST_F(AcquireFencePoolWindowIntegrationTest, ReservingAUsedFenceCanReleaseThePreparingPhaseOrStartAPeer) {
        vsl::Swapchain chain(device, surface, {}, 64, 48);
        for (bool start_peer : {false, true}) {
            SCOPED_TRACE(start_peer);
            auto producer = make_manager().independent(1), manager = make_manager().independent(1);
            vsl::SynchroManager synchronization(device);
            auto source = synchronization.create_fence("used", false);
            std::unique_ptr<vsl::DefaultPhase> phase;
            std::unique_ptr<vsl::ComputePhase> peer;
            bool invoked = false;
            const vsl_test::DeferredAction callback([&] {
                invoked = true;
                if (start_peer)
                    peer = std::make_unique<vsl::ComputePhase>(manager);
                else
                    phase.reset();
            });
            {
                auto prior = producer.start_phase<vsl::ComputePhase>(std::nullopt, std::nullopt, source);
                producer.get_buffer().retain_until_complete(callback.owner());
                ASSERT_EQ(prior.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
            }
            AcquireProbe probe;
            const vsl::operation::ScopedPresentationNativeCalls native(probe.native_calls());
            phase = std::make_unique<vsl::DefaultPhase>(manager, chain, std::nullopt, std::nullopt, std::nullopt, true, source);
            if (start_peer)
                EXPECT_THROW(phase->setup(), std::logic_error);
            else
                EXPECT_NO_THROW(phase->setup());
            EXPECT_TRUE(invoked);
            EXPECT_EQ(probe.calls, 0u);
            if (start_peer) {
                ASSERT_TRUE(peer);
                EXPECT_EQ(peer->completion().status(), vsl::Completion::Status::PENDING);
                peer->cancel();
            } else
                EXPECT_FALSE(phase);
            auto next = manager.start_phase<vsl::DefaultPhase>(chain, std::nullopt, std::nullopt, std::nullopt, false, source);
            next.cancel();
            EXPECT_TRUE(next.completion().wait());
            manager.wait_idle();
            producer.wait_idle();
        }
    }
    TEST_F(AcquireFencePoolWindowIntegrationTest, NativeAcquisitionSavesItsProofBeforeTheDeletedPhaseReleasesTheSource) {
        auto manager = make_manager().independent(1);
        const auto domain = vsl::lifetime::CommandAccess::Owner(manager.get_buffer());
        vsl::Swapchain chain(device, surface, {}, 64, 48);
        const auto registry = vsl::lifetime::presentation_domain(chain.data_);
        AcquireProbe probe;
        const vsl::operation::ScopedPresentationNativeCalls native(probe.native_calls());
        std::unique_ptr<vsl::DefaultPhase> phase;
        std::shared_ptr<vsl::sync::FenceProof> proof;
        phase = std::make_unique<vsl::DefaultPhase>(manager, chain, std::nullopt, std::nullopt, std::nullopt, true);
        probe.after_native = [&] {
            const auto evidence = registry->acquisitions.back();
            proof = {evidence, &evidence->proof};
            phase.reset();
        };
        EXPECT_NO_THROW(phase->setup());
        EXPECT_FALSE(phase);
        ASSERT_TRUE(proof);
        EXPECT_EQ(proof->status, vsl::sync::FenceProofStatus::COMPLETE);
        ASSERT_TRUE(domain->acquire_fences);
        EXPECT_TRUE(std::holds_alternative<std::monostate>(domain->acquire_fences->uses[0].use));
        vsl::FenceHolder{domain->acquire_fences}.reset();
        EXPECT_TRUE(vsl::sync::poll_fence_proof(*proof));
        manager.wait_idle();
    }

    TEST_F(PresentPhaseWindowIntegrationTest, PreparationCanReleaseTheCurrentPostponedPhaseBeforeAcquisition) {
        make_surface();
        ASSERT_FALSE(HasFatalFailure());
        if (!physical.data_->device.getSurfaceSupportKHR(queue_family, surface->data_->surface))
            GTEST_SKIP() << "Graphics queue cannot present";
        auto manager = make_manager().independent(1);
        vsl::Swapchain chain(device, surface, {}, 64, 48);
        std::unique_ptr<vsl::DefaultPhase> pending;
        bool invoked = false;
        const vsl_test::DeferredAction callback([&] {
            invoked = true;
            pending.reset();
        });
        {
            auto previous = manager.start_phase<vsl::ComputePhase>();
            manager.get_buffer().retain_until_complete(callback.owner());
            ASSERT_EQ(previous.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
        }
        pending = std::make_unique<vsl::DefaultPhase>(manager, chain, std::nullopt, std::nullopt, std::nullopt, true);
        EXPECT_NO_THROW(pending->setup());
        EXPECT_TRUE(invoked);
        EXPECT_FALSE(pending);
        auto next = manager.start_phase<vsl::DefaultPhase>(chain);
        next.cancel();
        EXPECT_TRUE(next.completion().wait());
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
            if (!frame) {
                manager.wait_idle();
                const auto& domains = vsl::lifetime::CommandAccess::Owner(manager.get_buffer())->presentation_domains;
                EXPECT_TRUE(std::ranges::all_of(domains, [](const auto& domain) { return domain->records.empty(); }));
            }
            EXPECT_EQ(phase.present(), VSL_NAMESPACE::ComputePhase::SubmitResult::SUCCESS);
            const auto& domains = vsl::lifetime::CommandAccess::Owner(manager.get_buffer())->presentation_domains;
            EXPECT_TRUE(std::ranges::any_of(domains, [](const auto& domain) { return !domain->records.empty(); }));
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
            const auto owned = manager.get_buffer().data_->recording_context->work->owned_fence;
            phase << VSL_NAMESPACE::command::RenderPassBegin(pass, target); // 終了コマンドを省略し、記録途中のキャンセルを確かめる。
            phase.cancel();
            EXPECT_EQ(vsl::lifetime::CommandAccess::Owner(manager.get_buffer())->submissions.at(phase.frame_slot)->owned_fence, owned);
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
    TEST_F(PresentPhaseWindowIntegrationTest, CancelCallbacksCanDestroyThePhaseAfterItsImageIsReturned) {
        make_surface();
        ASSERT_FALSE(HasFatalFailure());
        if (!physical.data_->device.getSurfaceSupportKHR(queue_family, surface->data_->surface))
            GTEST_SKIP() << "Graphics queue cannot present";
        vsl::Swapchain chain(device, surface, std::nullopt, 64, 48);
        for (const bool previous_slot : {false, true}) {
            auto manager = make_manager().independent(1);
            std::unique_ptr<vsl::DefaultPhase> phase;
            vsl::Completion returned;
            bool rejected = false;
            const vsl_test::DeferredAction callback([&] {
                returned = phase->completion();
                try {
                    auto other = manager.start_phase<vsl::ComputePhase>();
                } catch (const std::logic_error&) {
                    rejected = true;
                }
                phase.reset();
            });
            if (previous_slot) {
                auto first = manager.start_phase<vsl::ComputePhase>();
                manager.get_buffer().retain(callback.owner());
                ASSERT_EQ(first.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
                ASSERT_TRUE(first.completion().wait());
            }
            phase = std::make_unique<vsl::DefaultPhase>(manager, chain);
            const auto discarded = phase->completion();
            if (!previous_slot)
                manager.get_buffer().retain_until_complete(callback.owner());
            phase->cancel();
            EXPECT_FALSE(phase);
            EXPECT_TRUE(rejected);
            EXPECT_EQ(discarded.status(), vsl::Completion::Status::CANCELLED);
            EXPECT_NE(discarded.data_, returned.data_);
            EXPECT_TRUE(returned.wait());
            vsl::DefaultPhase next(manager, chain);
            next.cancel();
            EXPECT_TRUE(next.completion().wait());
        }
    }
    TEST_F(PresentPhaseWindowIntegrationTest, PreviousSlotReleaseCanDestroyThePhaseAfterNormalPresentation) {
        make_surface();
        ASSERT_FALSE(HasFatalFailure());
        if (!physical.data_->device.getSurfaceSupportKHR(queue_family, surface->data_->surface))
            GTEST_SKIP() << "Graphics queue cannot present";
        vsl::Swapchain chain(device, surface, std::nullopt, 64, 48);
        auto manager = make_manager().independent(1);
        vsl::RenderPass pass(chain);
        vsl::FrameBuffer target(chain, pass);
        std::unique_ptr<vsl::DefaultPhase> phase;
        vsl::Completion presented;
        const vsl_test::DeferredAction callback([&] {
            presented = phase->completion();
            phase.reset();
        });
        {
            auto first = manager.start_phase<vsl::ComputePhase>();
            manager.get_buffer().retain(callback.owner());
            ASSERT_EQ(first.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
            ASSERT_TRUE(first.completion().wait());
        }
        phase = std::make_unique<vsl::DefaultPhase>(manager, chain);
        *phase << vsl::command::RenderPassBegin(pass, target) << vsl::command::RenderPassEnd{};
        EXPECT_EQ(phase->submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
        EXPECT_FALSE(phase);
        EXPECT_TRUE(presented.wait());
        vsl::DefaultPhase next(manager, chain);
        next.cancel();
        EXPECT_TRUE(next.completion().wait());
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
    TEST_F(PresentPhaseWindowIntegrationTest, WSIMarkersProtectPendingAcquireAndPlannedPresentAcrossWaitIdle) {
        make_surface();
        ASSERT_FALSE(HasFatalFailure());
        if (!physical.data_->device.getSurfaceSupportKHR(queue_family, surface->data_->surface))
            GTEST_SKIP() << "Graphics queue cannot present";
        vsl::Swapchain chain(device, surface, std::nullopt, 64, 48);
        vsl::RenderPass pass(chain);
        vsl::FrameBuffer target(chain, pass);
        auto commands = make_manager().independent(1);
        vsl::SynchroManager sync(device);
        auto available = sync.create_semaphore("available");
        auto finished = sync.create_semaphore("finished", chain.data_->swap_chain_images.size());
        auto fence = sync.create_fence("complete", false);
        vsl::DefaultPhase phase(commands, chain, available, finished, fence);
        commands.wait_idle();
        EXPECT_THROW(available.destroy(), std::logic_error);
        EXPECT_THROW(finished.destroy(), std::logic_error);
        EXPECT_THROW(fence.wait(std::uint64_t{0}), std::logic_error);
        phase << vsl::command::RenderPassBegin(pass, target) << vsl::command::RenderPassEnd{};
        ASSERT_EQ(phase.submit_commands(), vsl::ComputePhase::SubmitResult::SUCCESS);
        const auto result = phase.completion();
        ASSERT_TRUE(result.wait());
        commands.wait_idle();
        EXPECT_NO_THROW(available.destroy());
        EXPECT_THROW(finished.destroy(), std::logic_error);
        EXPECT_NO_THROW(fence.reset());
        EXPECT_NO_THROW(fence.destroy());
        EXPECT_EQ(result.status(), vsl::Completion::Status::COMPLETE);
        ASSERT_EQ(phase.present(), vsl::ComputePhase::SubmitResult::SUCCESS);
        EXPECT_THROW(finished.destroy(), std::logic_error);
        commands.wait_idle();
        EXPECT_NO_THROW(finished.destroy());
    }
    TEST_F(PresentPhaseWindowIntegrationTest, CancelTransfersReservationAndOriginalSemaphoresBeforeNotifyingTheOldResult) {
        make_surface();
        ASSERT_FALSE(HasFatalFailure());
        if (!physical.data_->device.getSurfaceSupportKHR(queue_family, surface->data_->surface))
            GTEST_SKIP() << "Graphics queue cannot present";
        vsl::Swapchain chain(device, surface, std::nullopt, 64, 48);
        auto commands = make_manager().independent(1);
        vsl::SynchroManager sync(device);
        auto available = sync.create_semaphore("original available");
        auto finished = sync.create_semaphore("original finished", chain.data_->swap_chain_images.size());
        auto fence = sync.create_fence("original fence", false);
        auto phase = std::make_unique<vsl::DefaultPhase>(commands, chain, available, finished, fence);
        const auto discarded = phase->completion();
        const auto reservation = vsl::sync::current_fence_reservation(fence.data_->uses[0]);
        vsl::Completion returned;
        bool invoked = false;
        const vsl_test::DeferredAction callback([&] {
            invoked = true;
            returned = phase->completion();
            EXPECT_EQ(discarded.status(), vsl::Completion::Status::CANCELLED);
            EXPECT_EQ(reservation->result.lock(), returned.data_);
            EXPECT_NE(reservation->result.lock(), discarded.data_);
            const auto& work = vsl::lifetime::CommandAccess::Owner(commands.get_buffer())->submissions.at(0);
            EXPECT_EQ(work->fence_reservation, reservation);
            EXPECT_EQ(std::get<std::weak_ptr<vsl::impl::SubmissionImpl>>(fence.data_->uses[0].use).lock(), returned.data_);
            EXPECT_EQ(work->wait_semaphore, available.data_.get());
            EXPECT_EQ(work->signal_semaphore, finished.data_.get());
            phase.reset();
        });
        commands.get_buffer().retain_until_complete(callback.owner());
        auto unused_available = sync.create_semaphore("new field available");
        auto unused_finished = sync.create_semaphore("new field finished", chain.data_->swap_chain_images.size());
        auto unused_fence = sync.create_fence("new field fence", false);
        phase->next_image_available = unused_available;
        phase->calculation_finish = unused_finished;
        phase->in_flight_fence = unused_fence;
        phase->cancel();
        EXPECT_TRUE(invoked);
        EXPECT_FALSE(phase);
        ASSERT_TRUE(returned.wait());
        EXPECT_EQ(discarded.status(), vsl::Completion::Status::CANCELLED);
        EXPECT_EQ(reservation->result.lock(), returned.data_);
        EXPECT_NO_THROW(unused_available.destroy());
        EXPECT_NO_THROW(unused_finished.destroy());
        EXPECT_NO_THROW(unused_fence.destroy());
        commands.wait_idle();
        EXPECT_NO_THROW(available.destroy());
        EXPECT_NO_THROW(finished.destroy());
        EXPECT_NO_THROW(fence.destroy());
    }
    TEST_F(PresentPhaseWindowIntegrationTest, FailedSetupLeavesNoFenceReservationOrWSIMarker) {
        make_surface();
        ASSERT_FALSE(HasFatalFailure());
        if (!physical.data_->device.getSurfaceSupportKHR(queue_family, surface->data_->surface))
            GTEST_SKIP() << "Graphics queue cannot present";
        vsl::Swapchain chain(device, surface, std::nullopt, 64, 48);
        if (chain.data_->swap_chain_images.size() < 2)
            GTEST_SKIP() << "A too-small present semaphore array requires multiple images";
        auto commands = make_manager().independent(1);
        vsl::SynchroManager sync(device);
        auto available = sync.create_semaphore("available");
        auto finished = sync.create_semaphore("too small");
        auto fence = sync.create_fence("completion", false);
        EXPECT_THROW((vsl::DefaultPhase(commands, chain, available, finished, fence)), std::invalid_argument);
        EXPECT_NO_THROW(available.destroy());
        EXPECT_NO_THROW(finished.destroy());
        EXPECT_NO_THROW(fence.destroy());
        EXPECT_FALSE(commands.get_buffer().completion());
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
