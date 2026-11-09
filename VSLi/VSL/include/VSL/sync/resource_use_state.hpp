#pragma once
#include "completion.hpp"
#include <vector>

namespace VSL_NAMESPACE {
    namespace sync {
        struct ResourceUseAccess;
    }
    // submissionや保持中のframeを所有せず、最終的な完了状態を観測する。
    class CompletionObservation {
        Completion _result;

    public:
        CompletionObservation() = default;
        explicit CompletionObservation(const Completion& completion);
        Completion::Status status() const;
        bool busy() const { return _result.busy(); }
        bool native_uncertain() const { return _result.native_uncertain(); }
        Completion::Failure failure() const { return _result.failure(); }
        std::int32_t error_code() const { return _result.error_code(); }
        explicit operator bool() const { return bool(_result); }
    };

    enum class UncertainUsePolicy {
        ALLOW_REUSE,
        REQUIRE_RECREATION
    };
    // CPU結果だけを保持する。結果からWorkへの参照はweakであり、資源を所有しない。
    class ResourceUseState {
        friend struct sync::ResourceUseAccess;
        mutable std::vector<std::shared_ptr<impl::SubmissionImpl>> _uses;
        UncertainUsePolicy _uncertain = UncertainUsePolicy::ALLOW_REUSE;
        void discard_finished() const;
        void require_known_use() const;
        void track_pending(const Completion& completion) const;

    public:
        ResourceUseState() = default;
        explicit ResourceUseState(UncertainUsePolicy policy) noexcept : _uncertain(policy) {}
        ResourceUseState(const ResourceUseState&) = delete;
        ResourceUseState& operator=(const ResourceUseState&) = delete;
        bool busy() const;
        bool wait(std::uint64_t timeout = UINT64_MAX) const;
        void require_idle() const;
        void check_available(const Completion& completion) const;
        void mark_used(Completion completion) const;
    };

} // namespace VSL_NAMESPACE
