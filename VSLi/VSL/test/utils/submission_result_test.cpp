#include <VSL/vulkan/_pimpls.hpp>
#include <VSL/sync/submission_completion.hpp>
#include <VSL/lifetime/submission_retention.hpp>
#include <gtest/gtest.h>

namespace {
    using Status = vsl::Completion::Status;
    using Failure = vsl::Completion::Failure;
    std::shared_ptr<vsl::impl::SubmissionWork> pending_work();
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
} // namespace
