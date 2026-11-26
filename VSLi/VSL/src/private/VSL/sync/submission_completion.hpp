#pragma once
#include <VSL/vulkan/pv.hpp>
#include <VSL/sync/completion.hpp>

namespace VSL_NAMESPACE::sync {
    struct SubmissionNativeState;
    // 結果を先に確定してterminal callbackを通知する。SUBMISSION保持はWork寿命。
    bool terminal_status(Completion::Status status) noexcept;
    // 複数結果を先に保存し、callbackはfinish_submissionで後から通知できる。
    bool save_terminal(impl::SubmissionWork& work, Completion::Status status,
                       Completion::Failure failure = Completion::Failure::NONE, std::int32_t error_code = 0) noexcept;
    void publish_terminal(impl::SubmissionWork& work, Completion::Status status,
                          Completion::Failure failure = Completion::Failure::NONE, std::int32_t error_code = 0) noexcept;
    void observe_submission_result(impl::SubmissionWork& work, std::int32_t native_result);
    void save_submit_result(impl::SubmissionImpl& result, std::int32_t native_result) noexcept;
    void save_submit_result(SubmissionNativeState& state, std::int32_t native_result) noexcept;
    void save_native_idle(impl::SubmissionWork& work, std::int32_t native_result) noexcept;
    void finish_submission(impl::SubmissionWork& submission) noexcept;
} // namespace VSL_NAMESPACE::sync
