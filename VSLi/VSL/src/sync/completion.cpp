#include <VSL/sync/completion.hpp>
#include <VSL/sync/submission_completion.hpp>
#include <VSL/vulkan/_pimpls.hpp>

namespace VSL_NAMESPACE {
    Completion::Status Completion::status() const {
        if (!data_)
            return Status::CANCELLED;
        if (data_->status == Status::SUBMITTED &&
            data_->device->device.getFenceStatus(data_->fence) == vk::Result::eSuccess)
            data_->status = Status::COMPLETE;
        if (data_->status == Status::COMPLETE && data_->release_resources)
            data_->release_resources(*data_);
        return data_->status;
    }
    bool Completion::ready() const {
        return status() == Status::COMPLETE;
    }
    bool Completion::wait(std::uint64_t timeout) const {
        auto current = status();
        if (current == Status::COMPLETE)
            return true;
        if (current != Status::SUBMITTED)
            return false;
        if (data_->device->device.waitForFences(data_->fence, true, timeout) != vk::Result::eSuccess)
            return false;
        data_->status = Status::COMPLETE;
        if (data_->release_resources)
            data_->release_resources(*data_);
        return true;
    }
} // namespace VSL_NAMESPACE

namespace VSL_NAMESPACE::sync {
    void finish_submission(impl::SubmissionImpl& submission) noexcept {
        if (submission.status == Completion::Status::SUBMITTED) {
            const auto result = submission.device->device.waitForFences(1, &submission.fence, true, UINT64_MAX);
            submission.status = result == vk::Result::eSuccess ? Completion::Status::COMPLETE : Completion::Status::CANCELLED;
        }
        if (submission.observed_status)
            *submission.observed_status = submission.status == Completion::Status::PENDING ? Completion::Status::CANCELLED : submission.status;
    }
} // namespace VSL_NAMESPACE::sync
