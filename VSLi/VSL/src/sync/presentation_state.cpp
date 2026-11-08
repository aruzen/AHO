#include <VSL/sync/presentation_state.hpp>
#include <VSL/operation/submission_ops.hpp>
#include <VSL/operation/presentation_ops.hpp>

namespace VSL_NAMESPACE::sync {
    namespace {
        void observe_fence_proof(FenceProof& proof, vk::Result result) noexcept;
        void observe_present_proof(PresentState& state, vk::Result result) noexcept;
        void observe_fence_proof(FenceProof& proof, vk::Result result) noexcept {
            proof.result = result;
            if (result == vk::Result::eSuccess)
                proof.status = FenceProofStatus::COMPLETE;
            else if (result == vk::Result::eErrorDeviceLost)
                proof.status = FenceProofStatus::DEVICE_LOST;
            else if (result != vk::Result::eNotReady && result != vk::Result::eTimeout)
                proof.status = FenceProofStatus::UNCERTAIN;
        }
        void observe_present_proof(PresentState& state, vk::Result result) noexcept {
            if (result == vk::Result::eSuccess)
                state.status = PresentStatus::COMPLETE;
            else if (result == vk::Result::eErrorDeviceLost) {
                state.result = result;
                state.status = PresentStatus::FAILED;
            } else if (result != vk::Result::eNotReady && result != vk::Result::eTimeout) {
                state.result = result;
                state.status = PresentStatus::UNCERTAIN;
            }
        }
    } // namespace
    bool fence_proof_terminal(const FenceProof& proof) noexcept {
        return proof.status == FenceProofStatus::COMPLETE || proof.status == FenceProofStatus::DEVICE_LOST || proof.status == FenceProofStatus::NOT_ACQUIRED;
    }
    bool poll_fence_proof(FenceProof& proof) noexcept {
        if ((proof.status == FenceProofStatus::PENDING || proof.status == FenceProofStatus::UNCERTAIN) && proof.fence)
            observe_fence_proof(proof, operation::poll_fence(proof.device, proof.fence));
        return proof.status == FenceProofStatus::COMPLETE;
    }
    bool wait_fence_proof(FenceProof& proof, std::uint64_t timeout) noexcept {
        if ((proof.status == FenceProofStatus::PENDING || proof.status == FenceProofStatus::UNCERTAIN) && proof.fence)
            observe_fence_proof(proof, operation::wait_fence(proof.device, proof.fence, timeout));
        return proof.status == FenceProofStatus::COMPLETE;
    }
    PresentEnqueue classify_present_result(vk::Result result) noexcept {
        switch (result) {
            case vk::Result::eSuccess:
            case vk::Result::eSuboptimalKHR:
            case vk::Result::eErrorOutOfDateKHR:
            case vk::Result::eErrorSurfaceLostKHR:
            case static_cast<vk::Result>(VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT):
                return PresentEnqueue::ENQUEUED;
            case vk::Result::eErrorOutOfHostMemory:
            case vk::Result::eErrorOutOfDeviceMemory:
                return PresentEnqueue::NOT_ENQUEUED;
            case vk::Result::eErrorDeviceLost:
                return PresentEnqueue::DEVICE_LOST;
            default:
                return PresentEnqueue::UNKNOWN;
        }
    }
    PresentEnqueue commit_present(PresentState& state, vk::Result result) noexcept {
        state.result = result;
        const auto outcome = classify_present_result(result);
        switch (outcome) {
            case PresentEnqueue::ENQUEUED:
                state.status = PresentStatus::SUBMITTED;
                break;
            case PresentEnqueue::NOT_ENQUEUED:
                state.status = PresentStatus::PENDING;
                break;
            case PresentEnqueue::DEVICE_LOST:
                state.status = PresentStatus::FAILED;
                break;
            case PresentEnqueue::UNKNOWN:
                state.status = PresentStatus::UNCERTAIN;
                break;
        }
        return outcome;
    }
    bool present_terminal(const PresentState& state) noexcept {
        return state.status == PresentStatus::COMPLETE || state.status == PresentStatus::FAILED || state.status == PresentStatus::CANCELLED || state.status == PresentStatus::LEGACY_RELEASED;
    }
    void fail_present(PresentState& state, vk::Result result) noexcept {
        if (present_terminal(state))
            return;
        state.result = result;
        state.status = PresentStatus::FAILED;
    }
    bool poll_present(PresentState& state) noexcept {
        if ((state.status == PresentStatus::SUBMITTED || state.status == PresentStatus::UNCERTAIN) && state.strict && state.proof)
            observe_present_proof(state, operation::poll_present_fence(state.device, state.proof));
        else if (state.status == PresentStatus::SUBMITTED && state.reacquire) {
            if (poll_fence_proof(*state.reacquire))
                state.status = PresentStatus::COMPLETE;
            else if (state.reacquire->status == FenceProofStatus::DEVICE_LOST)
                observe_present_proof(state, vk::Result::eErrorDeviceLost);
        }
        return state.status == PresentStatus::COMPLETE;
    }
    bool wait_present(PresentState& state, std::uint64_t timeout) noexcept {
        if (state.status == PresentStatus::SUBMITTED && state.strict && state.proof)
            observe_present_proof(state, operation::wait_present_fence(state.device, state.proof, timeout));
        else if (state.status == PresentStatus::SUBMITTED && state.reacquire) {
            if (wait_fence_proof(*state.reacquire, timeout))
                state.status = PresentStatus::COMPLETE;
            else if (state.reacquire->status == FenceProofStatus::DEVICE_LOST)
                observe_present_proof(state, vk::Result::eErrorDeviceLost);
        } else if (state.status == PresentStatus::UNCERTAIN)
            poll_present(state); // enqueue不明のFenceは無限待機しない。
        return state.status == PresentStatus::COMPLETE;
    }
} // namespace VSL_NAMESPACE::sync
