#include <VSL/sync/completion.hpp>
#include <VSL/sync/submission_completion.hpp>
#include <VSL/operation/submission_ops.hpp>
#include <VSL/lifetime/submission_retention.hpp>
#include <VSL/sync/synchronization_state.hpp>
#include <VSL/vulkan/_pimpls.hpp>
#include <stdexcept>
#include <thread>
#include <utility>

namespace VSL_NAMESPACE::sync {
    namespace {
        void release_terminal_resources(impl::SubmissionWork& work) noexcept;
        void release_terminal_resources(impl::SubmissionWork& work) noexcept {
            if (work.result->native.active || native_pending(*work.result))
                return;
            const auto pin = work.quarantined ? lifetime::unquarantine_submission(work) : nullptr;
            if (work.quarantined)
                return;
            if (terminal_status(work.result->status))
                work.result->work.reset();
            if (const auto release = std::exchange(work.release_resources, nullptr))
                release(work);
        }
    } // namespace
    bool terminal_status(Completion::Status status) noexcept {
        return status == Completion::Status::COMPLETE || status == Completion::Status::CANCELLED || status == Completion::Status::FAILED;
    }
    bool save_terminal(impl::SubmissionWork& work, Completion::Status status, Completion::Failure failure, std::int32_t error_code) noexcept {
        if (terminal_status(work.result->status))
            return false;
        work.result->failure = failure;
        work.result->error_code = error_code;
        work.result->status = status;
        if (!work.result->native.active && !native_pending(*work.result) && !work.quarantined)
            work.result->work.reset();
        return true;
    }
    void publish_terminal(impl::SubmissionWork& work, Completion::Status status, Completion::Failure failure, std::int32_t error_code) noexcept {
        if (save_terminal(work, status, failure, error_code))
            release_terminal_resources(work);
    }
    void observe_submission_result(impl::SubmissionWork& work, std::int32_t native_result) {
        if (work.result->native.active || (work.result->status != Completion::Status::SUBMITTED && !native_pending(*work.result)))
            return;
        if (work.device && work.device->lost)
            native_result = static_cast<std::int32_t>(vk::Result::eErrorDeviceLost);
        const auto result = static_cast<vk::Result>(native_result);
        if (result == vk::Result::eSuccess || result == vk::Result::eErrorDeviceLost) {
            save_native_idle(work, native_result);
            release_terminal_resources(work);
        } else if (result != vk::Result::eNotReady && result != vk::Result::eTimeout)
            throw std::runtime_error("Submission completion observation failed: " + vk::to_string(result));
    }
    void save_submit_result(impl::SubmissionImpl& result, std::int32_t native_result) noexcept {
        const auto native = static_cast<vk::Result>(native_result);
        result.native.observation = native_result;
        result.native.complete = native != vk::Result::eSuccess && native != vk::Result::eErrorUnknown;
        if (native == vk::Result::eSuccess)
            result.native.enqueue = SubmissionEnqueue::ENQUEUED;
        else if (native == vk::Result::eErrorOutOfHostMemory || native == vk::Result::eErrorOutOfDeviceMemory)
            result.native.enqueue = SubmissionEnqueue::NOT_ENQUEUED;
        else if (native == vk::Result::eErrorDeviceLost)
            result.native.enqueue = SubmissionEnqueue::DEVICE_LOST;
        else {
            result.native.enqueue = SubmissionEnqueue::UNKNOWN;
            result.native.complete = false;
        }
    }
    void save_native_idle(impl::SubmissionWork& work, std::int32_t native_result) noexcept {
        if (work.device && work.device->lost)
            native_result = static_cast<std::int32_t>(vk::Result::eErrorDeviceLost);
        const auto result = static_cast<vk::Result>(native_result);
        if (result != vk::Result::eSuccess && result != vk::Result::eErrorDeviceLost)
            return;
        work.result->native.observation = native_result;
        work.result->native.complete = true;
        if (result == vk::Result::eErrorDeviceLost && work.device)
            work.device->lost = true;
        if (work.result->status == Completion::Status::SUBMITTED)
            save_terminal(work, result == vk::Result::eSuccess ? Completion::Status::COMPLETE : Completion::Status::FAILED,
                          result == vk::Result::eSuccess ? Completion::Failure::NONE : Completion::Failure::DEVICE_LOST,
                          result == vk::Result::eSuccess ? 0 : native_result);
        else if (terminal_status(work.result->status) && !work.result->native.active && !work.quarantined)
            work.result->work.reset();
    }
    void finish_submission(impl::SubmissionWork& work) noexcept {
        if (work.result->native.active) {
            work.abandon_requested = true;
            return;
        }
        if (native_pending(*work.result) && work.result->native.enqueue == SubmissionEnqueue::UNKNOWN) {
            const auto result = work.device->lost ? vk::Result::eErrorDeviceLost : operation::poll_fence(work.device->device, work.fence);
            if (result == vk::Result::eSuccess || result == vk::Result::eErrorDeviceLost)
                save_native_idle(work, static_cast<std::int32_t>(result));
            release_terminal_resources(work);
            return;
        }
        if (work.result->status == Completion::Status::PENDING)
            publish_terminal(work, Completion::Status::CANCELLED);
        while (work.result->status == Completion::Status::SUBMITTED) {
            auto result = work.device->lost ? vk::Result::eErrorDeviceLost : operation::wait_fence(work.device->device, work.fence, UINT64_MAX);
            if (result != vk::Result::eSuccess && result != vk::Result::eErrorDeviceLost && result != vk::Result::eTimeout)
                result = operation::wait_device_idle(work.device->device);
            if (result == vk::Result::eSuccess)
                observe_submission_result(work, static_cast<std::int32_t>(result));
            else if (result == vk::Result::eErrorDeviceLost)
                observe_submission_result(work, static_cast<std::int32_t>(result));
            else
                std::this_thread::yield(); // 完了を確定できない間は資源を破棄しない。
        }
        release_terminal_resources(work);
    }
} // namespace VSL_NAMESPACE::sync

namespace VSL_NAMESPACE {
    namespace {
        Completion::Status query_result(const std::shared_ptr<impl::SubmissionImpl>& result);
        Completion::Status query_result(const std::shared_ptr<impl::SubmissionImpl>& result) {
            if (!result)
                return Completion::Status::CANCELLED;
            if (result->native.active)
                return result->status;
            if (auto work = result->work.lock()) {
                if (result->status == Completion::Status::SUBMITTED || sync::native_pending(*result))
                    sync::observe_submission_result(*work, static_cast<std::int32_t>(work->device->lost ? vk::Result::eErrorDeviceLost : operation::poll_fence(work->device->device, work->fence)));
                else if (work->quarantined && sync::terminal_status(result->status))
                    sync::finish_submission(*work);
            }
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
    bool Completion::busy() const {
        const auto result = data_;
        (void)query_result(result);
        return result && sync::pending_use(*result);
    }
    bool Completion::native_uncertain() const {
        const auto result = data_;
        return result && result->native.enqueue == sync::SubmissionEnqueue::UNKNOWN;
    }
    bool Completion::wait(std::uint64_t timeout) const {
        const auto result = data_;
        if (result && result->native.active)
            return false;
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
