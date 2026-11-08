#pragma once
#include <VSL/vulkan/pv.hpp>
#include <memory>
#include <vector>

namespace VSL_NAMESPACE::sync {
    struct FenceReservation {
        std::weak_ptr<impl::SubmissionImpl> result;
    };
    struct FenceUseState {
        std::weak_ptr<FenceReservation> reservation;
        std::weak_ptr<impl::SubmissionImpl> submitted;
        bool accessing = false;
    };
    struct SemaphoreWSIUse {
        std::weak_ptr<impl::SubmissionImpl> acquisition;
        bool acquire = false, complete = false;
    };
    struct SemaphoreUseState {
        std::vector<std::weak_ptr<impl::SubmissionImpl>> graphics;
        std::vector<std::weak_ptr<SemaphoreWSIUse>> wsi;
        bool accessing = false;
    };
    bool pending_or_submitted(const std::shared_ptr<impl::SubmissionImpl>& result);
    void track_semaphore(impl::SemaphoreImpl& semaphore, const std::shared_ptr<impl::SubmissionImpl>& result);
    std::shared_ptr<SemaphoreWSIUse> track_semaphore_wsi(impl::SemaphoreImpl& semaphore, const std::shared_ptr<impl::SubmissionImpl>& acquisition = {});
    void require_semaphore_idle(SemaphoreUseState& state);
} // namespace VSL_NAMESPACE::sync
