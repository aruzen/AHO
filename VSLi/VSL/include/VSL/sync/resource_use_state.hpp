#pragma once
#include "completion.hpp"
#include <vector>

namespace VSL_NAMESPACE {
    // submissionや保持中のframeを所有せず、最終的な完了状態を観測する。
    class CompletionObservation {
        std::weak_ptr<impl::SubmissionImpl> _submission;
        std::shared_ptr<const Completion::Status> _terminal;

    public:
        CompletionObservation() = default;
        explicit CompletionObservation(const Completion& completion);
        Completion::Status status() const;
        explicit operator bool() const { return bool(_terminal); }
    };

    // 弱参照により、この状態を含むframeをsubmission側から保持できる。
    class ResourceUseState {
        mutable std::vector<std::weak_ptr<impl::SubmissionImpl>> _uses;
        void discard_finished() const;

    public:
        ResourceUseState() = default;
        ResourceUseState(const ResourceUseState&) = delete;
        ResourceUseState& operator=(const ResourceUseState&) = delete;
        bool busy() const;
        bool wait(std::uint64_t timeout = UINT64_MAX) const;
        void require_idle() const;
        void check_available(const Completion& completion) const;
        void mark_used(Completion completion) const;
    };

} // namespace VSL_NAMESPACE
