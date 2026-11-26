#pragma once
#include "command.hpp"
#include <memory>
#include <span>
#include <vector>

namespace VSL_NAMESPACE {
    // Observes the terminal result without keeping the submission or its retained frame alive.
    class CompletionObservation {
        std::weak_ptr<impl::SubmissionImpl> _submission;
        std::shared_ptr<const Completion::Status> _terminal;

    public:
        CompletionObservation() = default;
        explicit CompletionObservation(const Completion& completion);
        Completion::Status status() const;
        explicit operator bool() const { return bool(_terminal); }
    };

    // Weak observations also allow a submission to retain a frame containing this state.
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

    struct ResourceUse {
        std::shared_ptr<ResourceUseState> state;
        // Snapshot of the actual resource, excluding state and its Completions.
        std::shared_ptr<void> owner;
        void record(const CommandBuffer& buffer) const;
    };

    // Descriptor bindings are read at recording time, including updates made after bind_resources().
    class ResourceUseGroup {
        ResourceUseState _state;
        std::shared_ptr<void> _owner;
        std::vector<std::vector<ResourceUse>> _bindings;

    public:
        explicit ResourceUseGroup(size_t bindings, std::shared_ptr<void> owner = {});
        void require_idle() const;
        void replace(size_t binding, std::vector<ResourceUse> resources);
        void record(const CommandBuffer& buffer) const;
    };
} // namespace VSL_NAMESPACE
