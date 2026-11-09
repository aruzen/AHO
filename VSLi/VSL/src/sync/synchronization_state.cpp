#include <VSL/sync/synchronization_state.hpp>
#include <VSL/sync/submission_completion.hpp>
#include <VSL/vulkan/_pimpls.hpp>
#include <algorithm>
#include <stdexcept>

namespace VSL_NAMESPACE::sync {
    bool native_pending(const impl::SubmissionImpl& result) noexcept {
        return !result.native.complete && (result.native.enqueue == SubmissionEnqueue::ENQUEUED || result.native.enqueue == SubmissionEnqueue::UNKNOWN);
    }
    bool pending_use(const impl::SubmissionImpl& result) noexcept {
        return result.status == Completion::Status::PENDING || result.status == Completion::Status::SUBMITTED ||
               result.native.active || native_pending(result);
    }
    bool recordable(const impl::SubmissionImpl& result) noexcept {
        return result.status == Completion::Status::PENDING && !result.native.active && result.native.enqueue == SubmissionEnqueue::NOT_CALLED;
    }
    std::shared_ptr<FenceReservation> current_fence_reservation(const FenceUseState& state) noexcept {
        const auto* reservation = std::get_if<std::weak_ptr<FenceReservation>>(&state.use);
        return reservation ? reservation->lock() : nullptr;
    }
    void mark_fence_submitted(impl::FenceImpl& fence, std::uint32_t index, const std::shared_ptr<impl::SubmissionImpl>& result) {
        fence.uses.at(index).use = std::weak_ptr<impl::SubmissionImpl>(result);
    }
    bool pending_or_submitted(const std::shared_ptr<impl::SubmissionImpl>& result) {
        if (!result)
            return false;
        (void)Completion{result}.status();
        return pending_use(*result);
    }
    void require_semaphore_element(const impl::SemaphoreImpl& semaphore, std::uint32_t index) {
        if (index >= semaphore.semaphores.size() || semaphore.use.poisoned.size() != semaphore.semaphores.size())
            throw std::out_of_range("Semaphore element is unavailable");
        if (semaphore.use.poisoned[index])
            throw std::logic_error("Semaphore state is uncertain; recreate its native group");
    }
    void poison_semaphore_element(impl::SemaphoreImpl& semaphore, std::uint32_t index) noexcept {
        if (index < semaphore.use.poisoned.size())
            semaphore.use.poisoned[index] = true;
    }
    void track_semaphore(impl::SemaphoreImpl& semaphore, const std::shared_ptr<impl::SubmissionImpl>& result) {
        if (!result || semaphore.semaphores.empty() || semaphore.use.accessing)
            throw std::logic_error("Semaphore is unavailable for recording");
        auto& uses = semaphore.use.graphics;
        std::erase_if(uses, [](const auto& weak) {
            const auto value = weak.lock();
            return !value || !pending_use(*value);
        });
        if (!std::ranges::any_of(uses, [&](const auto& weak) { return weak.lock() == result; }))
            uses.push_back(result);
    }
    std::shared_ptr<SemaphoreWSIUse> track_semaphore_wsi(impl::SemaphoreImpl& semaphore, const std::shared_ptr<impl::SubmissionImpl>& acquisition) {
        if (semaphore.semaphores.empty() || semaphore.use.accessing)
            throw std::logic_error("Semaphore is unavailable for presentation");
        auto marker = std::make_shared<SemaphoreWSIUse>();
        marker->acquire = bool(acquisition);
        marker->acquisition = acquisition;
        std::erase_if(semaphore.use.wsi, [](const auto& weak) {
            const auto use = weak.lock();
            return !use || use->complete;
        });
        semaphore.use.wsi.push_back(marker);
        return marker;
    }
    void require_semaphore_idle(SemaphoreUseState& state) {
        std::vector<std::shared_ptr<impl::SubmissionImpl>> graphics;
        std::vector<std::shared_ptr<SemaphoreWSIUse>> wsi;
        graphics.reserve(state.graphics.size());
        wsi.reserve(state.wsi.size());
        for (const auto& weak : state.graphics)
            if (auto result = weak.lock())
                graphics.push_back(std::move(result));
        for (const auto& weak : state.wsi)
            if (auto marker = weak.lock())
                wsi.push_back(std::move(marker));
        for (const auto& result : graphics)
            if (pending_or_submitted(result))
                throw std::logic_error("Semaphore is in graphics use");
        for (const auto& marker : wsi) {
            if (marker->complete)
                continue;
            if (marker->acquire)
                if (const auto result = marker->acquisition.lock()) {
                    const auto status = Completion{result}.status();
                    if (status == Completion::Status::COMPLETE ||
                        (status == Completion::Status::FAILED && result->failure == Completion::Failure::DEVICE_LOST) ||
                        (result->native.complete && result->native.observation == static_cast<std::int32_t>(vk::Result::eErrorDeviceLost)))
                        marker->complete = true; // graphicsのwait完了はacquire signal完了も証明する。
                }
            if (!marker->complete)
                throw std::logic_error("Semaphore is in acquire or presentation use");
        }
    }
} // namespace VSL_NAMESPACE::sync
