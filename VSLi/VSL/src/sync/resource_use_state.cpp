#include <VSL/sync/resource_use_state.hpp>
#include <VSL/vulkan/_pimpls.hpp>
#include <algorithm>
#include <chrono>
#include <stdexcept>

namespace VSL_NAMESPACE {
    CompletionObservation::CompletionObservation(const Completion& completion) : _result(completion) {}
    Completion::Status CompletionObservation::status() const {
        return _result.status();
    }

    void ResourceUseState::discard_finished() const {
        // 完了callbackは同じ状態へ再入できるため、pollとlive一覧の変更を分離する。
        if (std::ranges::any_of(_uses, [](const auto& use) { const auto result = use.lock(); return result && result->status == Completion::Status::SUBMITTED; })) {
            const auto observed = _uses;
            for (const auto& use : observed)
                (void)Completion{use.lock()}.status();
        }
        std::erase_if(_uses, [](const auto& use) {
            const auto result = use.lock();
            return !result || (result->status != Completion::Status::PENDING && result->status != Completion::Status::SUBMITTED);
        });
    }
    bool ResourceUseState::busy() const {
        discard_finished();
        return !_uses.empty();
    }
    bool ResourceUseState::wait(std::uint64_t timeout) const {
        discard_finished();
        if (std::ranges::any_of(_uses, [](const auto& use) { const auto result = use.lock(); return result && result->status == Completion::Status::PENDING; }))
            return false;
        const auto observed = _uses;
        const auto start = std::chrono::steady_clock::now();
        for (const auto& use : observed) {
            const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - start).count();
            const auto remaining = timeout == UINT64_MAX ? UINT64_MAX : (std::uint64_t(elapsed) < timeout ? timeout - std::uint64_t(elapsed) : 0);
            const auto completion = Completion{use.lock()};
            if (completion && !completion.wait(remaining))
                return false;
        }
        discard_finished();
        return _uses.empty();
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
    void ResourceUseState::track_pending(const Completion& completion) const {
        if (!completion || completion.data_->status != Completion::Status::PENDING)
            throw std::logic_error("Resource use requires a recording phase");
        std::erase_if(_uses, [](const auto& use) {
            const auto result = use.lock();
            return !result || (result->status != Completion::Status::PENDING && result->status != Completion::Status::SUBMITTED);
        });
        if (!std::ranges::any_of(_uses, [&](const auto& use) { return use.lock() == completion.data_; }))
            _uses.push_back(completion.data_);
    }
    void ResourceUseState::mark_used(Completion completion) const {
        track_pending(completion);
        // callback中も新しい記録が先に見えるようにする。観測失敗時も取消まで保持する。
        discard_finished();
        if (completion.status() != Completion::Status::PENDING)
            throw std::logic_error("Recording phase changed while tracking a resource");
    }
} // namespace VSL_NAMESPACE
