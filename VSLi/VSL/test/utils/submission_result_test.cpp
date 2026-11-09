#include <VSL/vulkan/_pimpls.hpp>
#include <VSL/sync/submission_completion.hpp>
#include <VSL/lifetime/submission_retention.hpp>
#include <VSL/operation/submission_ops.hpp>
#include <gtest/gtest.h>

namespace {
    using Status = vsl::Completion::Status;
    using Failure = vsl::Completion::Failure;
    struct CPUWorkScope {
        std::shared_ptr<vsl::impl::SubmissionWork> work;
        ~CPUWorkScope();
    };
    struct WaitObservation {
        std::weak_ptr<int> owner;
        unsigned waits = 0;
    };
    std::shared_ptr<vsl::impl::SubmissionWork> pending_work();
    CPUWorkScope::~CPUWorkScope() {
        work->result->native.active = false;
        work->result->native.complete = true;
        vsl::sync::save_terminal(*work, Status::CANCELLED);
        vsl::sync::finish_submission(*work);
    }
    std::shared_ptr<vsl::impl::SubmissionWork> pending_work() {
        auto work = std::make_shared<vsl::impl::SubmissionWork>();
        work->result->work = work;
        work->release_resources = vsl::lifetime::release_completed_resources;
        return work;
    }
    TEST(SubmissionResultTest, EmptyCompletionHasNoSuccessfulResultOrRetentionBoundary) {
        const vsl::Completion empty;
        EXPECT_FALSE(empty);
        EXPECT_EQ(empty.status(), Status::CANCELLED);
        EXPECT_EQ(empty.failure(), Failure::NONE);
        EXPECT_EQ(empty.error_code(), 0);
        EXPECT_FALSE(empty.ready());
        EXPECT_FALSE(empty.wait(0));
        EXPECT_THROW(empty.retain(std::make_shared<int>(7)), std::logic_error);
    }
    TEST(SubmissionResultTest, LogicalFailureStillWaitsForAKnownEnqueuedNativeUseBeforeReleasingOwners) {
        auto work = pending_work();
        const CPUWorkScope cleanup{work};
        work->device = std::make_shared<vsl::impl::LogicalDeviceImpl>();
        vsl::sync::save_submit_result(*work->result, VK_SUCCESS);
        vsl::sync::save_terminal(*work, Status::FAILED, Failure::SUBMIT_FAILED, VK_ERROR_UNKNOWN);
        WaitObservation observed;
        {
            auto owner = std::make_shared<int>(1);
            observed.owner = owner;
            work->completion_resources.push_back(owner);
        }
        const vsl::operation::ScopedSubmissionNativeCalls native({.context = &observed,
                                                                  .wait_fences = [](void* context, VkDevice, std::uint32_t, const VkFence*, VkBool32, std::uint64_t) noexcept {
                                                                      auto& observation = *static_cast<WaitObservation*>(context);
                                                                      EXPECT_FALSE(observation.owner.expired());
                                                                      ++observation.waits;
                                                                      return VK_SUCCESS;
                                                                  }});
        vsl::sync::finish_submission(*work);
        EXPECT_EQ(observed.waits, 1u);
        EXPECT_TRUE(observed.owner.expired());
        EXPECT_FALSE(vsl::sync::native_pending(*work->result));
        EXPECT_EQ(work->result->status, Status::FAILED);
        EXPECT_EQ(work->result->failure, Failure::SUBMIT_FAILED);
        EXPECT_EQ(work->result->error_code, VK_ERROR_UNKNOWN);
        vsl::sync::finish_submission(*work);
        EXPECT_EQ(observed.waits, 1u);
    }
    TEST(SubmissionResultTest, SubmitClassificationSeparatesLogicalFailureFromNativeUse) {
        for (const auto native : {VK_SUCCESS, VK_ERROR_OUT_OF_HOST_MEMORY, VK_ERROR_OUT_OF_DEVICE_MEMORY, VK_ERROR_DEVICE_LOST, VK_ERROR_UNKNOWN, VK_ERROR_VALIDATION_FAILED_EXT}) {
            SCOPED_TRACE(native);
            auto work = pending_work();
            work->result->native.active = true;
            const vsl::Completion result{work->result};
            vsl::sync::save_submit_result(*work->result, native);
            const bool uncertain = native == VK_ERROR_UNKNOWN || native == VK_ERROR_VALIDATION_FAILED_EXT;
            const bool queued = native == VK_SUCCESS;
            EXPECT_EQ(vsl::sync::native_pending(*work->result), queued || uncertain);
            EXPECT_EQ(result.native_uncertain(), uncertain);
            EXPECT_TRUE(result.busy());
            EXPECT_FALSE(result.wait(0));
            vsl::sync::save_terminal(*work, Status::FAILED, Failure::SUBMIT_FAILED, native);
            EXPECT_EQ(result.error_code(), native);
            vsl::sync::save_native_idle(*work, VK_SUCCESS);
            work->result->native.active = false;
            vsl::sync::finish_submission(*work);
            EXPECT_FALSE(result.busy());
            EXPECT_FALSE(result.wait());
            EXPECT_EQ(result.status(), Status::FAILED);
            EXPECT_EQ(result.error_code(), native);
            EXPECT_EQ(result.native_uncertain(), uncertain);
        }
    }
    TEST(SubmissionResultTest, UncertainHistoryKeepsOnlyCpuResultsAfterNativeCompletion) {
        auto work = pending_work();
        vsl::Completion result{work->result};
        std::weak_ptr<vsl::impl::SubmissionImpl> observation = work->result;
        vsl::ResourceUseState physical;
        vsl::ResourceUseState logical(vsl::UncertainUsePolicy::REQUIRE_RECREATION);
        physical.mark_used(result);
        logical.mark_used(result);
        std::weak_ptr<int> resource;
        {
            auto owner = std::make_shared<int>(7);
            resource = owner;
            work->completion_resources.push_back(owner);
        }
        work->result->native.active = true;
        vsl::sync::save_submit_result(*work->result, VK_ERROR_UNKNOWN);
        vsl::sync::save_terminal(*work, Status::FAILED, Failure::SUBMIT_FAILED, VK_ERROR_UNKNOWN);
        EXPECT_TRUE(physical.busy());
        EXPECT_TRUE(logical.busy());
        EXPECT_FALSE(resource.expired());
        vsl::sync::save_native_idle(*work, VK_SUCCESS);
        work->result->native.active = false;
        vsl::sync::finish_submission(*work);
        work.reset();
        EXPECT_TRUE(resource.expired());
        EXPECT_TRUE(result.data_->work.expired());
        result = {};
        EXPECT_FALSE(physical.busy());
        EXPECT_FALSE(logical.busy());
        EXPECT_FALSE(observation.expired());
        EXPECT_TRUE(logical.wait(0));
        auto next = pending_work();
        EXPECT_NO_THROW(physical.check_available({next->result}));
        EXPECT_THROW(logical.mark_used({next->result}), std::logic_error);
        EXPECT_THROW(logical.check_available({next->result}), std::logic_error);
        EXPECT_FALSE(logical.busy());
        EXPECT_EQ(vsl::Completion{observation.lock()}.status(), Status::FAILED);
    }
    TEST(SubmissionResultTest, CompletionCallbackCannotInvalidateTheCurrentAvailabilityCheck) {
        auto previous = pending_work(), current = pending_work();
        previous->device = std::make_shared<vsl::impl::LogicalDeviceImpl>();
        vsl::ResourceUseState use;
        use.mark_used({previous->result});
        previous->result->status = Status::SUBMITTED;
        previous->completion_resources.push_back(std::shared_ptr<void>(new int, [&](void* value) {
            vsl::sync::publish_terminal(*current, Status::CANCELLED);
            delete static_cast<int*>(value);
        }));
        const vsl::operation::ScopedSubmissionNativeCalls native({.fence_status = +[](void*, VkDevice, VkFence) noexcept { return VK_SUCCESS; }});
        const CPUWorkScope cleanup{previous};
        EXPECT_THROW(use.check_available({current->result}), std::logic_error);
        EXPECT_EQ(current->result->status, Status::CANCELLED);
    }
    TEST(SubmissionResultTest, CompletionCallbackCannotInsertUnknownHistoryDuringMarkUsed) {
        auto previous = pending_work(), current = pending_work(), peer = pending_work();
        previous->device = std::make_shared<vsl::impl::LogicalDeviceImpl>();
        vsl::ResourceUseState use(vsl::UncertainUsePolicy::REQUIRE_RECREATION);
        use.mark_used({previous->result});
        previous->result->status = Status::SUBMITTED;
        previous->completion_resources.push_back(std::shared_ptr<void>(new int, [&](void* value) {
            use.mark_used({peer->result});
            vsl::sync::save_submit_result(*peer->result, VK_ERROR_UNKNOWN);
            vsl::sync::save_terminal(*peer, Status::FAILED, Failure::SUBMIT_FAILED, VK_ERROR_UNKNOWN);
            vsl::sync::save_native_idle(*peer, VK_SUCCESS);
            delete static_cast<int*>(value);
        }));
        const vsl::operation::ScopedSubmissionNativeCalls native({.fence_status = +[](void*, VkDevice, VkFence) noexcept { return VK_SUCCESS; }});
        const CPUWorkScope cleanup{previous};
        EXPECT_THROW(use.mark_used({current->result}), std::logic_error);
        EXPECT_EQ(current->result->status, Status::PENDING);
        vsl::sync::publish_terminal(*current, Status::CANCELLED);
    }
    TEST(SubmissionResultTest, AvailabilityCheckKeepsItsOriginalCompletionAcrossCallbacks) {
        auto previous = pending_work(), current = pending_work();
        previous->device = std::make_shared<vsl::impl::LogicalDeviceImpl>();
        vsl::ResourceUseState use;
        use.mark_used({previous->result});
        previous->result->status = Status::SUBMITTED;
        vsl::Completion observed{current->result};
        previous->completion_resources.push_back(std::shared_ptr<void>(new int, [&](void* value) {
            observed = {};
            delete static_cast<int*>(value);
        }));
        const vsl::operation::ScopedSubmissionNativeCalls native({.fence_status = +[](void*, VkDevice, VkFence) noexcept { return VK_SUCCESS; }});
        const CPUWorkScope cleanup{previous};
        EXPECT_NO_THROW(use.check_available(observed));
        EXPECT_FALSE(observed);
        EXPECT_EQ(current->result->status, Status::PENDING);
        vsl::sync::publish_terminal(*current, Status::CANCELLED);
    }
    TEST(SubmissionResultTest, TerminalResultIsVisibleDuringReleaseAndCannotBeOverwritten) {
        vsl::Completion result;
        size_t releases = 0;
        auto work = pending_work();
        result = {work->result};
        const vsl::CompletionObservation observation(result);
        vsl::ResourceUseState use;
        use.mark_used(result);
        work->completion_resources.push_back(std::shared_ptr<void>(new int(7), [&](void* value) {
            EXPECT_EQ(result.status(), Status::FAILED);
            EXPECT_EQ(result.failure(), Failure::SUBMIT_FAILED);
            EXPECT_EQ(result.error_code(), -1);
            ++releases;
            delete static_cast<int*>(value);
        }));
        EXPECT_TRUE(use.busy());
        vsl::sync::publish_terminal(*work, Status::FAILED, Failure::SUBMIT_FAILED, -1);
        EXPECT_FALSE(use.busy());
        EXPECT_EQ(releases, 1u);
        vsl::sync::publish_terminal(*work, Status::COMPLETE);
        EXPECT_EQ(releases, 1u);
        work.reset();
        EXPECT_TRUE(result.data_->work.expired());
        EXPECT_EQ(observation.status(), Status::FAILED);
        EXPECT_EQ(observation.failure(), Failure::SUBMIT_FAILED);
        EXPECT_EQ(observation.error_code(), -1);
        EXPECT_EQ(result.status(), Status::FAILED);
        EXPECT_FALSE(result.wait(0));
    }
    TEST(SubmissionResultTest, TimeoutNotReadyAndUnknownErrorsPreserveTheResourcesUntilAnActualTerminalResult) {
        auto work = pending_work();
        const vsl::Completion result{work->result};
        std::weak_ptr<int> retained;
        {
            auto owner = std::make_shared<int>(7);
            retained = owner;
            work->completion_resources.push_back(owner);
        }
        work->result->status = Status::SUBMITTED;
        vsl::sync::observe_submission_result(*work, VK_NOT_READY);
        vsl::sync::observe_submission_result(*work, VK_TIMEOUT);
        EXPECT_EQ(work->result->status, Status::SUBMITTED);
        EXPECT_FALSE(retained.expired());
        EXPECT_THROW(vsl::sync::observe_submission_result(*work, VK_ERROR_OUT_OF_HOST_MEMORY), std::runtime_error);
        EXPECT_EQ(work->result->status, Status::SUBMITTED);
        EXPECT_FALSE(retained.expired());
        vsl::sync::observe_submission_result(*work, VK_ERROR_DEVICE_LOST);
        EXPECT_TRUE(retained.expired());
        EXPECT_EQ(result.status(), Status::FAILED);
        EXPECT_EQ(result.failure(), Failure::DEVICE_LOST);
        EXPECT_EQ(result.error_code(), VK_ERROR_DEVICE_LOST);
    }
    TEST(SubmissionResultTest, SavedCompletionAndObservationDoNotOwnTheLegacySubmissionResources) {
        auto work = pending_work();
        const vsl::Completion result{work->result};
        const vsl::CompletionObservation observation(result);
        std::weak_ptr<int> retained;
        {
            auto owner = std::make_shared<int>(7);
            retained = owner;
            result.retain(owner);
        }
        work->result->status = Status::SUBMITTED;
        vsl::sync::observe_submission_result(*work, VK_SUCCESS);
        EXPECT_TRUE(result.data_->work.expired());
        EXPECT_FALSE(retained.expired());
        work.reset();
        EXPECT_TRUE(retained.expired());
        EXPECT_TRUE(result.data_->work.expired());
        EXPECT_TRUE(result.ready());
        EXPECT_TRUE(result.wait(0));
        EXPECT_EQ(result.failure(), Failure::NONE);
        EXPECT_EQ(observation.status(), Status::COMPLETE);
    }
    TEST(SubmissionResultTest, BatchTerminalResultsPrecedeEveryReleaseAndAreReleasedOnce) {
        auto first = pending_work(), second = pending_work();
        first->result->status = second->result->status = Status::SUBMITTED;
        const vsl::Completion peer{second->result};
        unsigned releases = 0;
        first->completion_resources.push_back(std::shared_ptr<void>(new int(1), [&](void* value) {
            EXPECT_EQ(peer.status(), Status::FAILED);
            EXPECT_EQ(peer.failure(), Failure::DEVICE_LOST);
            ++releases;
            delete static_cast<int*>(value);
        }));
        EXPECT_TRUE(vsl::sync::save_terminal(*first, Status::FAILED, Failure::DEVICE_LOST, VK_ERROR_DEVICE_LOST));
        EXPECT_TRUE(vsl::sync::save_terminal(*second, Status::FAILED, Failure::DEVICE_LOST, VK_ERROR_DEVICE_LOST));
        EXPECT_EQ(releases, 0u);
        vsl::sync::finish_submission(*first);
        vsl::sync::finish_submission(*first);
        EXPECT_EQ(releases, 1u);
        EXPECT_FALSE(vsl::sync::save_terminal(*second, Status::COMPLETE));
    }
} // namespace
