#include <VSL/sync/synchronization_state.hpp>
#include <VSL/sync/submission_completion.hpp>
#include <VSL/vulkan/_pimpls.hpp>
#include <algorithm>
#include <stdexcept>

namespace VSL_NAMESPACE::sync {
    namespace {
        bool consumed_acquire(SemaphoreWSIUse& use) noexcept;
        bool same_lease(const SemaphoreWSIUse& first, const SemaphoreWSIUse& second) noexcept;
        bool is_predecessor(const SemaphoreWSIUse& marker, const SemaphoreWSIUse* own, size_t limit) noexcept;
        bool consumed_acquire(SemaphoreWSIUse& use) noexcept {
            if (use.complete)
                return true;
            if (use.acquire)
                if (const auto result = use.acquisition.lock(); result && result->native.complete && !result->native.active &&
                                                                (result->native.enqueue == SubmissionEnqueue::ENQUEUED ||
                                                                 result->native.observation == static_cast<std::int32_t>(vk::Result::eErrorDeviceLost)))
                    use.complete = true;
            return use.complete;
        }
        bool same_lease(const SemaphoreWSIUse& first, const SemaphoreWSIUse& second) noexcept {
            return !first.lease.expired() && !first.lease.owner_before(second.lease) && !second.lease.owner_before(first.lease);
        }
        bool is_predecessor(const SemaphoreWSIUse& marker, const SemaphoreWSIUse* own, size_t limit) noexcept {
            auto previous = own ? own->predecessor.lock() : nullptr;
            while (previous && limit--) {
                if (previous->source != marker.source || previous->element != marker.element)
                    return false;
                if (previous.get() == &marker)
                    return true;
                previous = previous->predecessor.lock();
            }
            return false;
        }
    } // namespace
    bool native_pending(const SubmissionNativeState& state) noexcept {
        return !state.complete && (state.enqueue == SubmissionEnqueue::ENQUEUED || state.enqueue == SubmissionEnqueue::UNKNOWN);
    }
    bool native_pending(const impl::SubmissionImpl& result) noexcept {
        return native_pending(result.native);
    }
    bool pending_use(const impl::SubmissionImpl& result) noexcept {
        return result.status == Completion::Status::PENDING || result.status == Completion::Status::SUBMITTED ||
               result.native.active || result.native.observing || native_pending(result);
    }
    bool recordable(const impl::SubmissionImpl& result) noexcept {
        return result.status == Completion::Status::PENDING && !result.native.active && !result.native.observing && result.native.enqueue == SubmissionEnqueue::NOT_CALLED;
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
    void require_acquire_signal(impl::SemaphoreImpl& semaphore, std::uint32_t index) {
        require_semaphore_element(semaphore, index);
        if (semaphore.use.accessing)
            throw std::logic_error("Semaphore is being accessed");
        for (const auto& weak : semaphore.use.wsi)
            if (const auto marker = weak.lock(); marker && !consumed_acquire(*marker) &&
                                                 (marker->element == index || (marker->acquire && marker->element == UINT32_MAX)))
                throw std::logic_error("Acquisition semaphore element still has an outstanding signal or wait");
    }
    void require_graphics_wait(const impl::SemaphoreImpl& semaphore, std::uint32_t index,
                               const std::shared_ptr<impl::SubmissionImpl>& result, const SemaphoreWSIUse* own) {
        require_semaphore_element(semaphore, index);
        for (const auto& weak : semaphore.use.wsi) {
            const auto marker = weak.lock();
            if (!marker || consumed_acquire(*marker) || marker->element != index)
                continue;
            if (own && same_lease(*marker, *own) && own->acquire && own->acquisition.lock() == result)
                continue;
            throw std::logic_error("Semaphore wait belongs to another acquisition");
        }
    }
    void require_graphics_signal(const impl::SemaphoreImpl& semaphore, std::uint32_t index,
                                 const std::shared_ptr<impl::SubmissionImpl>& result, const SemaphoreWSIUse* own) {
        require_semaphore_element(semaphore, index);
        for (const auto& weak : semaphore.use.wsi) {
            const auto marker = weak.lock();
            if (!marker || consumed_acquire(*marker) || marker->element != index)
                continue;
            if (own && ((same_lease(*marker, *own) && own->acquisition.lock() == result) || is_predecessor(*marker, own, semaphore.use.wsi.size())))
                continue;
            throw std::logic_error("Semaphore signal conflicts with an acquisition or presentation");
        }
    }
    void require_present_wait(const impl::SemaphoreImpl& semaphore, std::uint32_t index, const SemaphoreWSIUse& own) {
        require_semaphore_element(semaphore, index);
        for (const auto& weak : semaphore.use.wsi) {
            const auto marker = weak.lock();
            if (!marker || consumed_acquire(*marker) || marker->element != index)
                continue;
            if (marker.get() == &own || same_lease(*marker, own) || is_predecessor(*marker, &own, semaphore.use.wsi.size()))
                continue;
            throw std::logic_error("Presentation wait belongs to another acquisition");
        }
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
    std::shared_ptr<SemaphoreWSIUse> track_semaphore_wsi(impl::SemaphoreImpl& semaphore, const std::shared_ptr<impl::SubmissionImpl>& acquisition, std::uint32_t index) {
        if (semaphore.semaphores.empty() || semaphore.use.accessing)
            throw std::logic_error("Semaphore is unavailable for presentation");
        auto marker = std::make_shared<SemaphoreWSIUse>();
        marker->source = &semaphore;
        marker->acquire = bool(acquisition);
        marker->acquisition = acquisition;
        marker->element = index;
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
