#include <VSL/vulkan/resource_use.hpp>
#include <VSL/vulkan/_pimpls.hpp>
#include <algorithm>
#include <chrono>
#include <stdexcept>

namespace VSL_NAMESPACE {
    CompletionObservation::CompletionObservation(const Completion& completion) : _submission(completion.data_) {
        if (!completion)
            return;
        auto& terminal = completion.data_->observed_status;
        if (!terminal)
            terminal = std::make_shared<Completion::Status>(completion.status());
        _terminal = terminal;
    }
    Completion::Status CompletionObservation::status() const {
        if (auto submission = _submission.lock())
            return Completion{std::move(submission)}.status();
        return _terminal ? *_terminal : Completion::Status::CANCELLED;
    }

    void ResourceUseState::discard_finished() const {
        std::erase_if(_uses, [](const auto& use) {
            const auto status = Completion{use.lock()}.status();
            return status == Completion::Status::COMPLETE || status == Completion::Status::CANCELLED;
        });
    }
    bool ResourceUseState::busy() const {
        discard_finished();
        return !_uses.empty();
    }
    bool ResourceUseState::wait(std::uint64_t timeout) const {
        discard_finished();
        if (std::ranges::any_of(_uses, [](const auto& use) { return Completion{use.lock()}.status() == Completion::Status::PENDING; }))
            return false;
        const auto start = std::chrono::steady_clock::now();
        for (const auto& use : _uses) {
            const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - start).count();
            const auto remaining = timeout == UINT64_MAX ? UINT64_MAX : (std::uint64_t(elapsed) < timeout ? timeout - std::uint64_t(elapsed) : 0);
            const auto completion = Completion{use.lock()};
            if (completion && !completion.wait(remaining))
                return false;
        }
        discard_finished();
        return true;
    }
    void ResourceUseState::require_idle() const {
        if (busy())
            throw std::logic_error("Resource is still in use");
    }
    void ResourceUseState::check_available(const Completion& completion) const {
        if (!completion || completion.status() != Completion::Status::PENDING)
            throw std::logic_error("Resource use requires a recording phase");
        discard_finished();
        if (std::ranges::any_of(_uses, [&](const auto& use) { return use.lock() != completion.data_; }))
            throw std::logic_error("Resource is in use by another phase");
    }
    void ResourceUseState::mark_used(Completion completion) const {
        if (!completion || completion.status() != Completion::Status::PENDING)
            throw std::logic_error("Resource use requires a recording phase");
        discard_finished();
        if (!std::ranges::any_of(_uses, [&](const auto& use) { return use.lock() == completion.data_; }))
            _uses.push_back(completion.data_);
    }
    void ResourceUse::record(const CommandBuffer& buffer) const {
        if (!state && !owner)
            return;
        const auto completion = buffer.completion();
        if (!completion || completion.status() != Completion::Status::PENDING)
            throw std::logic_error("Tracked resources require a recording phase");
        if (owner)
            completion.retain(owner);
        if (state)
            state->mark_used(completion);
    }
    ResourceUseGroup::ResourceUseGroup(size_t bindings, std::shared_ptr<void> owner)
        : _owner(std::move(owner)), _bindings(bindings) {}
    void ResourceUseGroup::require_idle() const {
        _state.require_idle();
    }
    void ResourceUseGroup::replace(size_t binding, std::vector<ResourceUse> resources) {
        require_idle();
        _bindings.at(binding) = std::move(resources);
    }
    void ResourceUseGroup::record(const CommandBuffer& buffer) const {
        const auto completion = buffer.completion();
        if (!completion || completion.status() != Completion::Status::PENDING)
            throw std::logic_error("Tracked descriptor sets require a recording phase");
        if (_owner)
            completion.retain(_owner);
        _state.mark_used(completion);
        for (const auto& binding : _bindings)
            for (const auto& resource : binding)
                resource.record(buffer);
    }
} // namespace VSL_NAMESPACE
