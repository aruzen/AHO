#include <VSL/lifetime/command_access.hpp>
#include "support/vulkan_fixture.hpp"
#include "support/deferred_action.hpp"
#include <VSL/lifetime/synchronization_use.hpp>
#include <VSL/lifetime/command_domain.hpp>
#include <VSL/lifetime/submission_retention.hpp>
#include <VSL/sync/submission_completion.hpp>
#include <VSL/sync/presentation_state.hpp>
#include <VSL/operation/submission_ops.hpp>
#include <limits>

namespace {
    using VSL_NAMESPACE::lifetime::CommandAccess;
}
namespace {
    // queue未送信のunsignaled fenceでSUBMITTED判定を固定する。
    struct SubmittedFence {
        std::shared_ptr<vsl::impl::SubmissionWork> work;
        SubmittedFence(vsl::FenceHolder fence, std::uint32_t index);
        ~SubmittedFence();
    };
    class SynchronizeVulkanIntegrationTest : public vsl_test::VulkanIntegrationTest {
    protected:
        bool validate_vulkan() const override { return true; }
        std::optional<VSL_NAMESPACE::SynchroManager> manager;
        void SetUp() override {
            VulkanIntegrationTest::SetUp();
            if (!HasFatalFailure() && !IsSkipped())
                manager.emplace(device);
        }
        void TearDown() override;
    };
    SubmittedFence::SubmittedFence(vsl::FenceHolder fence, std::uint32_t index) : work(std::make_shared<vsl::impl::SubmissionWork>()) {
        work->device = fence.get_device().data_;
        work->external_fence = fence.data_.get();
        vsl::lifetime::retain_until_complete(*work, fence.data_);
        work->synchronization_owners[static_cast<size_t>(vsl::impl::SubmissionSource::FENCE)] = 0;
        work->fence = fence.data_->fences.at(index);
        work->fence_reservation = vsl::lifetime::reserve_fence(fence.data_, index, work->result);
        work->result->work = work;
        work->result->status = vsl::Completion::Status::SUBMITTED;
        vsl::sync::mark_fence_submitted(*fence.data_, index, work->result);
    }
    SubmittedFence::~SubmittedFence() {
        vsl::sync::publish_terminal(*work, vsl::Completion::Status::COMPLETE);
    }
    TEST_F(SynchronizeVulkanIntegrationTest, NamedLookupAndSemaphoreCounts) {
        EXPECT_FALSE(manager->get_fence("missing"));
        EXPECT_FALSE(manager->get_semaphore("missing"));
        EXPECT_FALSE((*manager)["missing"]);
        EXPECT_FALSE(manager->destroy_fence("missing"));
        EXPECT_FALSE(manager->destroy_semaphore("missing"));
        auto one = manager->create_semaphore("one"), many = manager->create_semaphore("many", 2);
        EXPECT_EQ(one.data_->semaphores.size(), 1u);
        EXPECT_EQ(many.data_->semaphores.size(), 2u);
        ASSERT_TRUE(manager->get_semaphore("many"));
        EXPECT_EQ(manager->get_semaphore("many")->data_, many.data_);
        EXPECT_NE(many.data_->semaphores[0], many.data_->semaphores[1]);
    }
    TEST_F(SynchronizeVulkanIntegrationTest, FenceWaitResetAndQueueSignalHaveObservableStates) {
        auto single = manager->create_fence("single", true);
        auto many = manager->create_fence("many", std::size_t{2}, true);
        ASSERT_EQ(single.data_->fences.size(), 1u);
        ASSERT_EQ(many.data_->fences.size(), 2u);
        EXPECT_EQ(manager->get_fence("single")->data_, single.data_);
        EXPECT_EQ((*manager)["many"]->data_, many.data_);
        EXPECT_TRUE(single.wait(std::uint64_t{0}));
        EXPECT_TRUE(many.wait(1, std::uint64_t{0}));
        single.reset();
        many.reset(1);
        EXPECT_FALSE(single.wait(std::uint64_t{0}));
        EXPECT_FALSE(many.wait(1, std::uint64_t{0}));
        auto queue = device.data_->device.getQueue(queue_family, 0);
        queue.submit(vk::SubmitInfo{}, single.data_->fences[0]);
        queue.submit(vk::SubmitInfo{}, many.data_->fences[1]);
        EXPECT_TRUE(single.wait());
        EXPECT_TRUE(many.wait(std::uint64_t{UINT64_MAX}));
    }
} // namespace

namespace {
    TEST_F(SynchronizeVulkanIntegrationTest, PendingFenceReservationsRejectWaitAndMutationButOtherElementsRemainIndependent) {
        auto fence = manager->create_fence("reserved", std::size_t{2}, false);
        auto first = make_manager().independent(1), second = make_manager().independent(2), contender = make_manager().independent(1);
        second.get_buffer().next();
        auto a = first.start_phase<vsl::ComputePhase>(std::nullopt, std::nullopt, fence);
        auto b = second.start_phase<vsl::ComputePhase>(std::nullopt, std::nullopt, fence);
        const auto old_token = vsl::sync::current_fence_reservation(fence.data_->uses[0]);
        EXPECT_THROW(contender.start_phase<vsl::ComputePhase>(std::nullopt, std::nullopt, fence), std::logic_error);
        EXPECT_THROW(fence.wait(std::uint64_t{0}), std::logic_error);
        EXPECT_THROW(fence.wait(0, 0), std::logic_error);
        EXPECT_THROW(fence.reset(), std::logic_error);
        EXPECT_THROW(fence.reset(1), std::logic_error);
        EXPECT_THROW(fence.destroy(), std::logic_error);
        a.cancel();
        EXPECT_NO_THROW(fence.reset(0));
        EXPECT_FALSE(fence.wait(0, 0));
        auto next = contender.start_phase<vsl::ComputePhase>(std::nullopt, std::nullopt, fence);
        EXPECT_NE(vsl::sync::current_fence_reservation(fence.data_->uses[0]), old_token);
        EXPECT_EQ(old_token->result.lock()->status, vsl::Completion::Status::CANCELLED);
        next.cancel();
        ASSERT_EQ(b.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
        ASSERT_TRUE(b.completion().wait());
        EXPECT_NO_THROW(fence.reset(1));
        EXPECT_EQ(b.completion().status(), vsl::Completion::Status::COMPLETE);
    }
    TEST_F(SynchronizeVulkanIntegrationTest, AcquireReservationsProtectOnlyTheirElementAndSaveProofBeforeReset) {
        auto fence = manager->create_fence("acquisition", std::size_t{2}, true);
        auto first = std::make_shared<vsl::sync::FenceProof>();
        vsl::lifetime::reserve_acquire_fence(fence.data_, 0, first);
        EXPECT_THROW(fence.wait(0, 0), std::logic_error);
        EXPECT_THROW(fence.reset(0), std::logic_error);
        EXPECT_THROW(fence.destroy(), std::logic_error);
        EXPECT_TRUE(fence.wait(1, 0));
        EXPECT_NO_THROW(fence.reset(1));
        vsl::lifetime::release_acquire_fence(*fence.data_, 0, first);
        EXPECT_THROW(fence.reset(0), std::logic_error);
        first->status = vsl::sync::FenceProofStatus::NOT_ACQUIRED;
        vsl::lifetime::release_acquire_fence(*fence.data_, 0, first);
        EXPECT_FALSE(fence.wait(0, 0));
        auto next = std::make_shared<vsl::sync::FenceProof>();
        vsl::lifetime::reserve_acquire_fence(fence.data_, 0, next);
        vsl::lifetime::release_acquire_fence(*fence.data_, 0, first);
        EXPECT_THROW(fence.reset(0), std::logic_error);
        device.data_->device.getQueue(queue_family, 0).submit(vk::SubmitInfo{}, fence.data_->fences[0]);
        next->status = vsl::sync::FenceProofStatus::PENDING;
        ASSERT_EQ(device.data_->device.waitForFences(fence.data_->fences[0], true, UINT64_MAX), vk::Result::eSuccess);
        EXPECT_NO_THROW(fence.reset(0));
        EXPECT_EQ(next->status, vsl::sync::FenceProofStatus::COMPLETE);
        EXPECT_FALSE(fence.wait(0, 0));
        EXPECT_TRUE(vsl::sync::poll_fence_proof(*next));
        EXPECT_EQ(first->status, vsl::sync::FenceProofStatus::NOT_ACQUIRED);
    }
    TEST_F(SynchronizeVulkanIntegrationTest, UnknownPresentEnqueueAllowsOnlyPollingAndRejectsReuse) {
        auto fence = manager->create_fence("unknown present", std::size_t{2});
        auto state = std::make_shared<vsl::sync::PresentState>();
        state->device = device.data_->device;
        state->strict = true;
        vsl::lifetime::reserve_present_fence(fence.data_, 0, state);
        EXPECT_THROW(vsl::lifetime::reserve_present_fence(fence.data_, 1, state), std::invalid_argument);
        EXPECT_EQ(state->proof, fence.data_->fences[0]);
        vsl::sync::commit_present(*state, vk::Result::eErrorUnknown);
        EXPECT_FALSE(fence.wait(0, 0));
        EXPECT_THROW(fence.wait(), std::logic_error);
        EXPECT_THROW(fence.reset(), std::logic_error);
        EXPECT_THROW(fence.destroy(), std::logic_error);
        EXPECT_THROW(make_manager().independent(1).start_phase<vsl::ComputePhase>(std::nullopt, std::nullopt, fence), std::logic_error);
        EXPECT_EQ(state->status, vsl::sync::PresentStatus::UNCERTAIN);
        // この状態機械testはNative presentを送っていないため、後処理だけ取消を保存する。
        state->status = vsl::sync::PresentStatus::CANCELLED;
        EXPECT_NO_THROW(fence.destroy());
    }
    TEST_F(SynchronizeVulkanIntegrationTest, AcquireReservationPinsTheOriginalProofAcrossGraphicsRelease) {
        auto fence = manager->create_fence("proof replacement");
        auto commands = make_manager().independent(1);
        auto requested = std::make_shared<vsl::sync::FenceProof>();
        const auto original = requested;
        bool invoked = false;
        const vsl_test::DeferredAction callback([&] {
            invoked = true;
            requested.reset();
        });
        {
            auto phase = commands.start_phase<vsl::ComputePhase>(std::nullopt, std::nullopt, fence);
            commands.get_buffer().retain_until_complete(callback.owner());
            ASSERT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
        }
        ASSERT_EQ(device.data_->device.waitForFences(fence.data_->fences[0], true, UINT64_MAX), vk::Result::eSuccess);
        vsl::lifetime::reserve_acquire_fence(fence.data_, 0, requested);
        EXPECT_TRUE(invoked);
        EXPECT_FALSE(requested);
        EXPECT_EQ(std::get<std::weak_ptr<vsl::sync::FenceProof>>(fence.data_->uses[0].use).lock(), original);
        EXPECT_THROW(fence.reset(), std::logic_error);
        original->status = vsl::sync::FenceProofStatus::NOT_ACQUIRED;
        vsl::lifetime::release_acquire_fence(*fence.data_, 0, original);
        EXPECT_NO_THROW(fence.reset());
    }
    TEST_F(SynchronizeVulkanIntegrationTest, FenceArrayLossIsSavedForEveryWorkBeforeReleaseCallbacks) {
        auto fence = manager->create_fence("batch", std::size_t{2});
        auto first = make_manager().independent(1), second = make_manager().independent(2);
        second.get_buffer().next();
        vsl::Completion a, b;
        unsigned releases = 0;
        const vsl_test::DeferredAction callback([&] {
            ++releases;
            EXPECT_EQ(a.status(), vsl::Completion::Status::FAILED);
            EXPECT_EQ(b.status(), vsl::Completion::Status::FAILED);
            EXPECT_EQ(a.failure(), vsl::Completion::Failure::DEVICE_LOST);
            EXPECT_EQ(b.failure(), vsl::Completion::Failure::DEVICE_LOST);
        });
        {
            auto phase = first.start_phase<vsl::ComputePhase>(std::nullopt, std::nullopt, fence);
            first.get_buffer().retain_until_complete(callback.owner());
            a = phase.completion();
            ASSERT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
        }
        {
            auto phase = second.start_phase<vsl::ComputePhase>(std::nullopt, std::nullopt, fence);
            b = phase.completion();
            ASSERT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
        }
        ASSERT_EQ(device.data_->device.waitForFences(fence.data_->fences, true, UINT64_MAX), vk::Result::eSuccess);
        const vsl::operation::ScopedSubmissionNativeCalls native({.wait_fences = +[](void*, VkDevice, std::uint32_t, const VkFence*, VkBool32, std::uint64_t) noexcept {
            return VK_ERROR_DEVICE_LOST;
        }});
        EXPECT_THROW(fence.wait(), vk::DeviceLostError);
        EXPECT_EQ(releases, 1u);
        EXPECT_EQ(a.error_code(), VK_ERROR_DEVICE_LOST);
        EXPECT_EQ(b.error_code(), VK_ERROR_DEVICE_LOST);
    }
    TEST_F(SynchronizeVulkanIntegrationTest, FenceArrayObservationPinsAllWorkAndBlocksRecursiveQueries) {
        auto fence = manager->create_fence("observed", std::size_t{2});
        auto first = make_manager().independent(1), second = make_manager().independent(2);
        second.get_buffer().next();
        auto a = std::make_unique<vsl::ComputePhase>(first, std::nullopt, std::nullopt, fence);
        auto b = second.start_phase<vsl::ComputePhase>(std::nullopt, std::nullopt, fence);
        auto ca = a->completion(), cb = b.completion();
        const std::weak_ptr<vsl::impl::CommandBufferImpl> ring = CommandAccess::Data(first.get_buffer());
        ASSERT_EQ(a->submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
        ASSERT_EQ(b.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
        struct Probe {
            vsl::Completion first, second;
            vsl::FenceHolder fence;
            unsigned calls = 0;
            std::function<void()> release_facades;
        } probe{ca, cb, fence};
        probe.release_facades = [&] {
            a.reset();
            first = vsl::CommandManager(CommandAccess::RawManager(nullptr));
            EXPECT_FALSE(ring.expired());
        };
        const vsl::operation::ScopedSubmissionNativeCalls native({.context = &probe, .wait_fences = +[](void* context, VkDevice device, std::uint32_t count, const VkFence* fences, VkBool32 all, std::uint64_t timeout) noexcept {
                                                                      auto& probe = *static_cast<Probe*>(context);
                                                                      ++probe.calls;
                                                                      probe.release_facades();
                                                                      EXPECT_EQ(probe.first.status(), vsl::Completion::Status::SUBMITTED);
                                                                      EXPECT_EQ(probe.second.status(), vsl::Completion::Status::SUBMITTED);
                                                                      EXPECT_FALSE(probe.first.wait(0));
                                                                      EXPECT_THROW(probe.fence.wait(std::uint64_t{0}), std::logic_error);
                                                                      EXPECT_THROW(probe.fence.reset(), std::logic_error);
                                                                      return vkWaitForFences(device, count, fences, all, timeout);
                                                                  }});
        EXPECT_TRUE(fence.wait());
        EXPECT_EQ(probe.calls, 1u);
        EXPECT_EQ(ca.status(), vsl::Completion::Status::COMPLETE);
        EXPECT_EQ(cb.status(), vsl::Completion::Status::COMPLETE);
        EXPECT_TRUE(ring.expired());
    }
    TEST_F(SynchronizeVulkanIntegrationTest, AcquisitionObservationBlocksFenceReuseAndKeepsASavedTerminalProof) {
        auto fence = manager->create_fence("proof", true);
        auto proof = std::make_shared<vsl::sync::FenceProof>();
        proof->device = device.data_->device;
        proof->fence = fence.data_->fences[0];
        proof->status = vsl::sync::FenceProofStatus::PENDING;
        fence.data_->uses[0].use = std::weak_ptr<vsl::sync::FenceProof>(proof);
        proof->status = vsl::sync::FenceProofStatus::UNCERTAIN;
        EXPECT_THROW(fence.wait(), std::logic_error);
        proof->status = vsl::sync::FenceProofStatus::PENDING;
        struct Probe {
            vsl::FenceHolder fence;
            std::shared_ptr<vsl::sync::FenceProof> proof;
            unsigned calls = 0;
        } probe{fence, proof};
        const vsl::operation::ScopedSubmissionNativeCalls native({.context = &probe, .fence_status = +[](void* context, VkDevice, VkFence) noexcept {
                                                                      auto& probe = *static_cast<Probe*>(context);
                                                                      if (++probe.calls > 1)
                                                                          return VK_NOT_READY;
                                                                      EXPECT_FALSE(vsl::sync::poll_fence_proof(*probe.proof));
                                                                      EXPECT_THROW(probe.fence.reset(), std::logic_error);
                                                                      EXPECT_THROW(probe.fence.wait(std::uint64_t{0}), std::logic_error);
                                                                      vsl::sync::save_fence_observation(*probe.proof, vk::Result::eSuccess);
                                                                      vsl::lifetime::release_acquire_fence(*probe.fence.data_, 0, probe.proof);
                                                                      EXPECT_TRUE(std::holds_alternative<std::weak_ptr<vsl::sync::FenceProof>>(probe.fence.data_->uses[0].use));
                                                                      return VK_ERROR_UNKNOWN;
                                                                  }});
        EXPECT_TRUE(vsl::sync::poll_fence_proof(*proof));
        EXPECT_EQ(proof->status, vsl::sync::FenceProofStatus::COMPLETE);
        EXPECT_EQ(probe.calls, 1u);
        EXPECT_NO_THROW(fence.reset());
    }
    TEST_F(SynchronizeVulkanIntegrationTest, WSISemaphoreLeasesSeparateElementsAndPreserveTheReacquireAncestry) {
        auto source = manager->create_semaphore("leased", 2);
        const auto result = std::make_shared<vsl::impl::SubmissionImpl>();
        const auto lease = std::make_shared<int>(1);
        auto acquire = vsl::sync::track_semaphore_wsi(*source.data_, result, 0);
        acquire->lease = lease;
        EXPECT_THROW(vsl::sync::require_acquire_signal(*source.data_, 0), std::logic_error);
        EXPECT_NO_THROW(vsl::sync::require_acquire_signal(*source.data_, 1));
        EXPECT_NO_THROW(vsl::sync::require_graphics_wait(*source.data_, 0, result, acquire.get()));
        EXPECT_THROW(vsl::sync::require_graphics_wait(*source.data_, 0, std::make_shared<vsl::impl::SubmissionImpl>()), std::logic_error);
        result->status = vsl::Completion::Status::FAILED;
        result->native.enqueue = vsl::sync::SubmissionEnqueue::NOT_ENQUEUED;
        result->native.complete = true;
        EXPECT_THROW(vsl::sync::require_acquire_signal(*source.data_, 0), std::logic_error);
        result->native.enqueue = vsl::sync::SubmissionEnqueue::ENQUEUED;
        EXPECT_NO_THROW(vsl::sync::require_acquire_signal(*source.data_, 0));
        auto first = vsl::sync::track_semaphore_wsi(*source.data_, {}, 1);
        auto second = vsl::sync::track_semaphore_wsi(*source.data_, {}, 1);
        auto third = vsl::sync::track_semaphore_wsi(*source.data_, {}, 1);
        first->lease = std::make_shared<int>(2);
        second->predecessor = first;
        third->predecessor = second;
        third->lease = lease;
        third->acquisition = result;
        EXPECT_NO_THROW(vsl::sync::require_graphics_signal(*source.data_, 1, result, third.get()));
        EXPECT_NO_THROW(vsl::sync::require_present_wait(*source.data_, 1, *third));
        auto unrelated = vsl::sync::track_semaphore_wsi(*source.data_, {}, 1);
        EXPECT_THROW(vsl::sync::require_graphics_signal(*source.data_, 1, result, third.get()), std::logic_error);
        first->complete = second->complete = third->complete = unrelated->complete = true;
    }
    TEST_F(SynchronizeVulkanIntegrationTest, AllFenceMutationsCheckEveryElementBeforeNativeChanges) {
        auto fence = manager->create_fence("array", std::size_t{2}, true);
        fence.reset(1);
        SubmittedFence pending(fence, 1);
        const auto handles = fence.data_->fences;
        EXPECT_THROW(fence.reset(), std::logic_error);
        EXPECT_THROW(fence.destroy(), std::logic_error);
        EXPECT_EQ(fence.data_->fences, handles);
        EXPECT_TRUE(fence.wait(0, 0));
        EXPECT_FALSE(fence.wait(std::uint64_t{0}));
        EXPECT_NO_THROW(fence.reset(0));
        EXPECT_FALSE(fence.wait(0, 0));
    }
    TEST_F(SynchronizeVulkanIntegrationTest, FenceCallbacksUseTheOriginalOwnerAndPreserveNamedReplacements) {
        for (const int operation : {0, 1, 2}) {
            auto commands = make_manager().independent(1);
            auto holder = manager->create_fence("replace", false);
            const auto alias = holder;
            bool invoked = false;
            const vsl_test::DeferredAction callback([&] {
                invoked = true;
                auto reentrant = alias;
                EXPECT_THROW(reentrant.wait(std::uint64_t{0}), std::logic_error);
                EXPECT_THROW(reentrant.reset(), std::logic_error);
                EXPECT_THROW(reentrant.destroy(), std::logic_error);
                holder = manager->create_fence("replace", true);
            });
            vsl::Completion result;
            {
                auto phase = commands.start_phase<vsl::ComputePhase>(std::nullopt, std::nullopt, holder);
                commands.get_buffer().retain_until_complete(callback.owner());
                result = phase.completion();
                ASSERT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
            }
            device.data_->device.waitIdle();
            if (operation == 0)
                EXPECT_TRUE(holder.wait());
            else if (operation == 1)
                holder.reset();
            else
                holder.destroy();
            EXPECT_TRUE(invoked);
            EXPECT_NE(holder.data_, alias.data_);
            ASSERT_TRUE(manager->get_fence("replace"));
            EXPECT_EQ(manager->get_fence("replace")->data_, holder.data_);
            EXPECT_TRUE(holder.wait(std::uint64_t{0}));
            EXPECT_EQ(result.status(), vsl::Completion::Status::COMPLETE);
            if (operation == 1) {
                auto old = alias;
                EXPECT_FALSE(old.wait(std::uint64_t{0}));
            }
            if (operation == 2)
                EXPECT_TRUE(alias.data_->fences.empty());
        }
    }
    TEST_F(SynchronizeVulkanIntegrationTest, SemaphoreTracksEveryPendingUseAndReleasesTerminalUsesWithoutOwnerCountHeuristics) {
        auto semaphore = manager->create_semaphore("shared");
        auto producer = make_manager().independent(1), consumer = make_manager().independent(1);
        auto first = producer.start_phase<vsl::ComputePhase>(std::nullopt, semaphore);
        auto later = consumer.start_phase<vsl::ComputePhase>(semaphore);
        EXPECT_THROW(semaphore.destroy(), std::logic_error);
        later.cancel();
        EXPECT_THROW(semaphore.destroy(), std::logic_error);
        ASSERT_EQ(first.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
        ASSERT_TRUE(first.completion().wait());
        EXPECT_GT(semaphore.data_.use_count(), 1);
        EXPECT_NO_THROW(semaphore.destroy());
        EXPECT_EQ(semaphore.size(), 0u);
    }
    TEST_F(SynchronizeVulkanIntegrationTest, SharedWaitAndSignalSourceRetainsOneOwnerAndOneUse) {
        auto semaphore = manager->create_semaphore("two roles");
        auto commands = make_manager().independent(1);
        auto phase = commands.start_phase<vsl::ComputePhase>(semaphore, semaphore);
        const auto* work = CommandAccess::Data(commands.get_buffer())->recording_context->work;
        ASSERT_TRUE(work);
        EXPECT_EQ(work->wait_semaphore, semaphore.data_.get());
        EXPECT_EQ(work->signal_semaphore, semaphore.data_.get());
        EXPECT_EQ(work->synchronization_owners[static_cast<size_t>(vsl::impl::SubmissionSource::WAIT_SEMAPHORE)],
                  work->synchronization_owners[static_cast<size_t>(vsl::impl::SubmissionSource::SIGNAL_SEMAPHORE)]);
        EXPECT_EQ(std::ranges::count_if(work->completion_resources, [&](const auto& owner) {
                      return !owner.owner_before(semaphore.data_) && !semaphore.data_.owner_before(owner);
                  }),
                  1);
        EXPECT_EQ(semaphore.data_->use.graphics.size(), 1u);
        phase.cancel();
        EXPECT_NO_THROW(semaphore.destroy());
    }
    TEST_F(SynchronizeVulkanIntegrationTest, SemaphoreSubmittedUseAndBusyNamedReplacementAreIndependent) {
        auto old = manager->create_semaphore("same");
        auto fence = manager->create_fence("probe", false);
        SubmittedFence pending(fence, 0);
        vsl::sync::track_semaphore(*old.data_, pending.work->result);
        auto replacement = manager->create_semaphore("same");
        EXPECT_THROW(old.destroy(), std::logic_error);
        ASSERT_TRUE(manager->get_semaphore("same"));
        EXPECT_EQ(manager->get_semaphore("same")->data_, replacement.data_);
        vsl::sync::publish_terminal(*pending.work, vsl::Completion::Status::COMPLETE);
        EXPECT_NO_THROW(old.destroy());
        EXPECT_EQ(manager->get_semaphore("same")->data_, replacement.data_);
    }
    TEST_F(SynchronizeVulkanIntegrationTest, SemaphoreDestroyCanReplaceItsHolderAndReleaseItsRegistryDuringCompletion) {
        auto holder = manager->create_semaphore("snapshot");
        const auto alias = holder;
        std::weak_ptr<vsl::impl::SynchroManagerImpl> registry = manager->data_;
        auto commands = make_manager().independent(1);
        bool invoked = false;
        const vsl_test::DeferredAction callback([&] {
            invoked = true;
            auto reentrant = alias;
            EXPECT_THROW(reentrant.destroy(), std::logic_error);
            holder = manager->create_semaphore("snapshot");
            manager.reset();
        });
        {
            auto phase = commands.start_phase<vsl::ComputePhase>(std::nullopt, holder);
            commands.get_buffer().retain_until_complete(callback.owner());
            ASSERT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
        }
        device.data_->device.waitIdle();
        holder.destroy();
        EXPECT_TRUE(invoked);
        EXPECT_TRUE(registry.expired());
        EXPECT_TRUE(alias.data_->semaphores.empty());
        EXPECT_EQ(holder.size(), 1u);
    }
    TEST_F(SynchronizeVulkanIntegrationTest, CreationValidationLeavesExistingNamesAndHandlesIntact) {
        auto fence = manager->create_fence("fence", true);
        auto semaphore = manager->create_semaphore("semaphore");
        for (const size_t count : {size_t{0}, std::numeric_limits<size_t>::max()}) {
            EXPECT_THROW(manager->create_fence("fence", count), std::invalid_argument);
            EXPECT_THROW(manager->create_semaphore("semaphore", count), std::invalid_argument);
            EXPECT_EQ(manager->get_fence("fence")->data_, fence.data_);
            EXPECT_EQ(manager->get_semaphore("semaphore")->data_, semaphore.data_);
        }
        EXPECT_TRUE(fence.wait(std::uint64_t{0}));
        EXPECT_EQ(semaphore.size(), 1u);
    }
    TEST_F(SynchronizeVulkanIntegrationTest, ForeignControlBlocksCannotBorrowOrDestroyTheSameNativeObjects) {
        auto fence = manager->create_fence("canonical", true);
        auto semaphore = manager->create_semaphore("canonical");
        vsl::FenceHolder forged_fence{std::shared_ptr<vsl::impl::FenceImpl>(std::make_shared<int>(0), fence.data_.get())};
        vsl::SemaphoreHolder forged_semaphore{std::shared_ptr<vsl::impl::SemaphoreImpl>(std::make_shared<int>(0), semaphore.data_.get())};
        EXPECT_THROW(forged_fence.wait(std::uint64_t{0}), std::invalid_argument);
        EXPECT_THROW(forged_fence.destroy(), std::invalid_argument);
        EXPECT_THROW(forged_semaphore.destroy(), std::invalid_argument);
        auto commands = make_manager().independent(1);
        EXPECT_THROW(commands.start_phase<vsl::ComputePhase>(std::nullopt, forged_semaphore), std::invalid_argument);
        EXPECT_THROW(commands.start_phase<vsl::ComputePhase>(std::nullopt, std::nullopt, forged_fence), std::invalid_argument);
        EXPECT_TRUE(fence.wait(std::uint64_t{0}));
        EXPECT_EQ(semaphore.size(), 1u);
    }
    TEST_F(SynchronizeVulkanIntegrationTest, ReservationWaitCallbackCannotResetDestroyOrStealTheElement) {
        auto fence = manager->create_fence("gate", std::size_t{2}, true);
        auto first = make_manager().independent(1), next = make_manager().independent(1), thief = make_manager().independent(1);
        bool invoked = false;
        const vsl_test::DeferredAction callback([&] {
            invoked = true;
            EXPECT_THROW(fence.reset(0), std::logic_error);
            EXPECT_THROW(fence.destroy(), std::logic_error);
            EXPECT_THROW(thief.start_phase<vsl::ComputePhase>(std::nullopt, std::nullopt, fence), std::logic_error);
            EXPECT_NO_THROW(fence.reset(1));
        });
        {
            auto phase = first.start_phase<vsl::ComputePhase>(std::nullopt, std::nullopt, fence);
            first.get_buffer().retain_until_complete(callback.owner());
            ASSERT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
        }
        device.data_->device.waitIdle();
        auto phase = next.start_phase<vsl::ComputePhase>(std::nullopt, std::nullopt, fence);
        EXPECT_TRUE(invoked);
        EXPECT_EQ(phase.completion().status(), vsl::Completion::Status::PENDING);
        phase.cancel();
    }
    TEST_F(SynchronizeVulkanIntegrationTest, FailedPreparationAndChangedFieldsDoNotLeavePendingUses) {
        auto commands = make_manager().independent(1);
        auto fence = manager->create_fence("failure", false);
        auto wait = manager->create_semaphore("wait");
        auto signal = manager->create_semaphore("signal");
        {
            auto phase = commands.start_phase<vsl::ComputePhase>(wait, signal, fence, false, static_cast<vsl::PipelineStage>(0));
            const auto result = phase.completion();
            EXPECT_THROW(phase.submit(), std::invalid_argument);
            EXPECT_EQ(result.status(), vsl::Completion::Status::FAILED);
        }
        EXPECT_NO_THROW(fence.reset());
        EXPECT_NO_THROW(wait.destroy());
        EXPECT_NO_THROW(signal.destroy());
        auto changed = commands.start_phase<vsl::ComputePhase>(std::nullopt, std::nullopt, fence);
        changed.in_flight_fence = manager->create_fence("new field", true);
        EXPECT_THROW(changed.submit(), std::logic_error);
        EXPECT_EQ(changed.completion().status(), vsl::Completion::Status::FAILED);
        EXPECT_NO_THROW(fence.destroy());
    }
} // namespace

namespace {
    TEST_F(SynchronizeVulkanIntegrationTest, DestroyIsIdempotentAndDoesNotEraseReplacement) {
        auto old = manager->create_fence("same", true);
        auto replacement = manager->create_fence("same", true);
        old.destroy();
        old.destroy();
        EXPECT_TRUE(old.data_->fences.empty());
        ASSERT_TRUE(manager->get_fence("same"));
        EXPECT_EQ(manager->get_fence("same")->data_, replacement.data_);
        EXPECT_TRUE(replacement.wait(std::uint64_t{0}));
        EXPECT_TRUE(manager->destroy_fence("same"));
        EXPECT_FALSE(manager->destroy_fence("same"));
        EXPECT_THROW(replacement.wait(std::uint64_t{0}), std::logic_error);
        auto semaphore = manager->create_semaphore("signal", 2);
        auto alias = semaphore;
        semaphore.destroy();
        alias.destroy();
        EXPECT_EQ(alias.size(), 0u);
        EXPECT_FALSE(manager->get_semaphore("signal"));
    }
    TEST_F(SynchronizeVulkanIntegrationTest, HoldersOutliveManagerWithoutOwningIt) {
        auto fence = manager->create_fence("survivor", true);
        auto semaphore = manager->create_semaphore("survivor");
        std::weak_ptr<VSL_NAMESPACE::impl::SynchroManagerImpl> weak = manager->data_;
        manager.reset();
        EXPECT_TRUE(weak.expired());
        EXPECT_TRUE(fence.wait(std::uint64_t{0}));
        semaphore.destroy();
        fence.destroy();
    }
} // namespace

namespace {
    TEST_F(SynchronizeVulkanIntegrationTest, SemaphoreDestroyRemovesOnlyItsNameAndDisarmsAllAliases) {
        auto value = manager->create_semaphore("a", 2);
        auto alias = value;
        auto retained = manager->create_semaphore("b");
        value.destroy();
        EXPECT_TRUE(alias.data_->semaphores.empty());
        EXPECT_FALSE(manager->get_semaphore("a"));
        ASSERT_TRUE(manager->get_semaphore("b"));
        EXPECT_EQ(manager->get_semaphore("b")->data_, retained.data_);
        // 検証に失敗しても、後続の確認でハンドルを二重破棄しないようにする。
        alias.data_->semaphores.clear();
        alias.destroy();
        VSL_NAMESPACE::SemaphoreHolder{}.destroy();
        EXPECT_TRUE(manager->destroy_semaphore("b"));
        EXPECT_TRUE(retained.data_->semaphores.empty());
        retained.data_->semaphores.clear();
        EXPECT_FALSE(manager->destroy_semaphore("b"));
    }
    TEST_F(SynchronizeVulkanIntegrationTest, FenceDestroyRemovesOnlyItsNameAndDisarmsAllAliases) {
        auto value = manager->create_fence("a", std::size_t{2}, true);
        auto alias = value;
        auto retained = manager->create_fence("b", true);
        value.destroy();
        EXPECT_TRUE(alias.data_->fences.empty());
        EXPECT_FALSE(manager->get_fence("a"));
        ASSERT_TRUE(manager->get_fence("b"));
        EXPECT_TRUE(retained.wait(std::uint64_t{0}));
        alias.data_->fences.clear();
        alias.destroy();
        VSL_NAMESPACE::FenceHolder{}.destroy();
        EXPECT_TRUE(manager->destroy_fence("b"));
        EXPECT_TRUE(retained.data_->fences.empty());
        retained.data_->fences.clear();
        EXPECT_FALSE(manager->destroy_fence("b"));
    }
    TEST_F(SynchronizeVulkanIntegrationTest, DestroyingReplacedHoldersKeepsTheCurrentNamedObjectsAlive) {
        auto old_semaphore = manager->create_semaphore("same");
        auto old_fence = manager->create_fence("same", true);
        auto current_semaphore = manager->create_semaphore("same");
        auto current_fence = manager->create_fence("same", true);
        old_semaphore.destroy();
        old_fence.destroy();
        EXPECT_TRUE(old_semaphore.data_->semaphores.empty());
        EXPECT_TRUE(old_fence.data_->fences.empty());
        ASSERT_TRUE(manager->get_semaphore("same"));
        ASSERT_TRUE(manager->get_fence("same"));
        EXPECT_EQ(manager->get_semaphore("same")->data_, current_semaphore.data_);
        EXPECT_EQ(manager->get_fence("same")->data_, current_fence.data_);
        EXPECT_TRUE(current_fence.wait(std::uint64_t{0}));
    }

    void SynchronizeVulkanIntegrationTest::TearDown() {
        if (manager) {
            device.data_->device.waitIdle();
            manager.reset();
        }
        VulkanIntegrationTest::TearDown();
    }
} // namespace
