#pragma once
#include <VSL/vulkan/pv.hpp>
#include <memory>
#include <vector>
#include <variant>
#include <cstdint>

namespace VSL_NAMESPACE::sync {
    enum class SubmissionEnqueue {
        NOT_CALLED,
        ENQUEUED,
        NOT_ENQUEUED,
        DEVICE_LOST,
        UNKNOWN
    };
    struct SubmissionNativeState {
        SubmissionEnqueue enqueue = SubmissionEnqueue::NOT_CALLED;
        bool active = false, complete = false, observing = false;
        std::int32_t observation = 0;
    };
    bool native_pending(const SubmissionNativeState& state) noexcept;
    bool native_pending(const impl::SubmissionImpl& result) noexcept;
    bool pending_use(const impl::SubmissionImpl& result) noexcept;
    bool recordable(const impl::SubmissionImpl& result) noexcept;
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
        impl::SemaphoreImpl* source = nullptr;
        std::weak_ptr<impl::SubmissionImpl> acquisition;
        std::weak_ptr<void> lease;
        std::weak_ptr<SemaphoreWSIUse> predecessor;
        std::uint32_t element = UINT32_MAX;
        bool acquire = false, complete = false;
    };
    struct SemaphoreUseState {
        std::vector<std::weak_ptr<impl::SubmissionImpl>> graphics;
        std::vector<std::weak_ptr<SemaphoreWSIUse>> wsi;
        std::vector<bool> poisoned;
        bool accessing = false;
    };
    bool pending_or_submitted(const std::shared_ptr<impl::SubmissionImpl>& result);
    void require_semaphore_element(const impl::SemaphoreImpl& semaphore, std::uint32_t index);
    void require_acquire_signal(impl::SemaphoreImpl& semaphore, std::uint32_t index);
    void require_graphics_wait(const impl::SemaphoreImpl& semaphore, std::uint32_t index,
                               const std::shared_ptr<impl::SubmissionImpl>& result, const SemaphoreWSIUse* own = nullptr);
    void require_graphics_signal(const impl::SemaphoreImpl& semaphore, std::uint32_t index,
                                 const std::shared_ptr<impl::SubmissionImpl>& result, const SemaphoreWSIUse* own = nullptr);
    void require_present_wait(const impl::SemaphoreImpl& semaphore, std::uint32_t index, const SemaphoreWSIUse& own);
    void poison_semaphore_element(impl::SemaphoreImpl& semaphore, std::uint32_t index) noexcept;
    void track_semaphore(impl::SemaphoreImpl& semaphore, const std::shared_ptr<impl::SubmissionImpl>& result);
    std::shared_ptr<SemaphoreWSIUse> track_semaphore_wsi(impl::SemaphoreImpl& semaphore, const std::shared_ptr<impl::SubmissionImpl>& acquisition = {}, std::uint32_t index = UINT32_MAX);
    void require_semaphore_idle(SemaphoreUseState& state);
} // namespace VSL_NAMESPACE::sync
