#pragma once
#include <VSL/vulkan/pv.hpp>
#include <memory>
#include <vector>
#include <variant>
#include <cstdint>

namespace VSL_NAMESPACE::sync {
    struct FenceProof;
    struct PresentState;
    struct FenceReservation {
        std::weak_ptr<impl::SubmissionImpl> result;
    };
    struct FenceUseState {
        // 一つのnative要素の現在の用途。過去の結果／proofは別のCPU観測値として残せる。
        using Use = std::variant<std::monostate, std::weak_ptr<FenceReservation>, std::weak_ptr<impl::SubmissionImpl>,
                                 std::weak_ptr<FenceProof>, std::weak_ptr<PresentState>>;
        Use use;
        bool accessing = false;
    };
    std::shared_ptr<FenceReservation> current_fence_reservation(const FenceUseState& state) noexcept;
    void mark_fence_submitted(impl::FenceImpl& fence, std::uint32_t index, const std::shared_ptr<impl::SubmissionImpl>& result);
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
