#include <VSL/sync/completion.hpp>
#include <VSL/sync/submission_completion.hpp>
#include <VSL/operation/submission_ops.hpp>
#include <VSL/vulkan/_pimpls.hpp>
#include <stdexcept>
#include <thread>

namespace VSL_NAMESPACE::sync {
    bool terminal_status(Completion::Status status) noexcept {
        return status == Completion::Status::COMPLETE || status == Completion::Status::CANCELLED || status == Completion::Status::FAILED;
    }
    void publish_terminal(impl::SubmissionWork& work, Completion::Status status, Completion::Failure failure, std::int32_t error_code) noexcept {
        if (terminal_status(work.result->status))
            return;
        work.result->failure = failure;
        work.result->error_code = error_code;
        work.result->status = status;
        work.result->work.reset();
        if (work.release_resources)
            work.release_resources(work);
    }
    void observe_submission_result(impl::SubmissionWork& work, std::int32_t native_result) {
        if (work.result->status != Completion::Status::SUBMITTED)
            return;
        const auto result = static_cast<vk::Result>(native_result);
        if (result == vk::Result::eSuccess)
            publish_terminal(work, Completion::Status::COMPLETE);
        else if (result == vk::Result::eErrorDeviceLost)
            publish_terminal(work, Completion::Status::FAILED, Completion::Failure::DEVICE_LOST, native_result);
        else if (result != vk::Result::eNotReady && result != vk::Result::eTimeout)
            throw std::runtime_error("Submission completion observation failed: " + vk::to_string(result));
    }
    void finish_submission(impl::SubmissionWork& work) noexcept {
        if (work.result->status == Completion::Status::PENDING)
            publish_terminal(work, Completion::Status::CANCELLED);
        while (work.result->status == Completion::Status::SUBMITTED) {
            auto result = operation::wait_fence(work.device->device, work.fence, UINT64_MAX);
            if (result != vk::Result::eSuccess && result != vk::Result::eErrorDeviceLost && result != vk::Result::eTimeout)
                result = operation::wait_device_idle(work.device->device);
            if (result == vk::Result::eSuccess)
                publish_terminal(work, Completion::Status::COMPLETE);
            else if (result == vk::Result::eErrorDeviceLost)
                publish_terminal(work, Completion::Status::FAILED, Completion::Failure::DEVICE_LOST, static_cast<std::int32_t>(result));
            else
                std::this_thread::yield(); // 完了を確定できない間は資源を破棄しない。
        }
    }
} // namespace VSL_NAMESPACE::sync

namespace VSL_NAMESPACE {
    namespace {
        Completion::Status query_result(const std::shared_ptr<impl::SubmissionImpl>& result);
        Completion::Status query_result(const std::shared_ptr<impl::SubmissionImpl>& result) {
            if (!result)
                return Completion::Status::CANCELLED;
            if (result->status != Completion::Status::SUBMITTED)
                return result->status;
            if (auto work = result->work.lock())
                sync::observe_submission_result(*work, static_cast<std::int32_t>(operation::poll_fence(work->device->device, work->fence)));
            return result->status;
        }
    } // namespace
    Completion::Status Completion::status() const {
        const auto result = data_;
        return query_result(result);
    }
    Completion::Failure Completion::failure() const {
        const auto result = data_;
        static_cast<void>(query_result(result));
        return result ? result->failure : Failure::NONE;
    }
    std::int32_t Completion::error_code() const {
        const auto result = data_;
        static_cast<void>(query_result(result));
        return result ? result->error_code : 0;
    }
    bool Completion::ready() const {
        return status() == Status::COMPLETE;
    }
    bool Completion::wait(std::uint64_t timeout) const {
        const auto result = data_;
        const auto current = query_result(result);
        if (current == Status::COMPLETE)
            return true;
        if (current != Status::SUBMITTED)
            return false;
        auto work = result->work.lock();
        if (!work)
            return result->status == Status::COMPLETE;
        sync::observe_submission_result(*work, static_cast<std::int32_t>(operation::wait_fence(work->device->device, work->fence, timeout)));
        return result->status == Status::COMPLETE;
    }
} // namespace VSL_NAMESPACE
