#include <VSL/sync/presentation_state.hpp>
#include <VSL/vulkan/_pimpls.hpp>
#include <VSL/operation/submission_ops.hpp>
#include <VSL/operation/presentation_ops.hpp>

namespace VSL_NAMESPACE::sync {
    namespace {
        struct ProofObservation {
            bool& observing;
            explicit ProofObservation(bool& observing) : observing(observing) { observing = true; }
            ~ProofObservation() { observing = false; }
        };
        void observe_fence_proof(FenceProof& proof, vk::Result result) noexcept;
        void observe_present_proof(PresentState& state, vk::Result result) noexcept;
        void observe_fence_proof(FenceProof& proof, vk::Result result) noexcept {
            if (proof.origin && proof.origin->lost)
                result = vk::Result::eErrorDeviceLost;
            if (result == vk::Result::eErrorDeviceLost && proof.origin)
                proof.origin->lost = true;
            if (fence_proof_terminal(proof))
                return;
            proof.result = result;
            if (result == vk::Result::eSuccess)
                proof.status = FenceProofStatus::COMPLETE;
            else if (result == vk::Result::eErrorDeviceLost) {
                proof.status = FenceProofStatus::DEVICE_LOST;
                if (proof.origin)
                    proof.origin->lost = true;
            } else if (result != vk::Result::eNotReady && result != vk::Result::eTimeout)
                proof.status = FenceProofStatus::UNCERTAIN;
        }
        void observe_present_proof(PresentState& state, vk::Result result) noexcept {
            if (state.origin && state.origin->lost)
                result = vk::Result::eErrorDeviceLost;
            if (result == vk::Result::eErrorDeviceLost && state.origin)
                state.origin->lost = true;
            if (present_terminal(state))
                return;
            if (result == vk::Result::eSuccess)
                state.status = PresentStatus::COMPLETE;
            else if (result == vk::Result::eErrorDeviceLost) {
                state.result = result;
                state.status = PresentStatus::FAILED;
                if (state.origin)
                    state.origin->lost = true;
            } else if (result != vk::Result::eNotReady && result != vk::Result::eTimeout) {
                state.result = result;
                state.status = PresentStatus::UNCERTAIN;
            }
        }
    } // namespace
    bool fence_proof_terminal(const FenceProof& proof) noexcept {
        return proof.status == FenceProofStatus::COMPLETE || proof.status == FenceProofStatus::DEVICE_LOST || proof.status == FenceProofStatus::NOT_ACQUIRED;
    }
    void save_fence_observation(FenceProof& proof, vk::Result result) noexcept {
        if (proof.status == FenceProofStatus::PENDING || proof.status == FenceProofStatus::UNCERTAIN)
            observe_fence_proof(proof, result);
    }
    void save_present_observation(PresentState& state, vk::Result result) noexcept {
        if (state.status == PresentStatus::SUBMITTED || state.status == PresentStatus::UNCERTAIN)
            observe_present_proof(state, result);
    }
    bool poll_fence_proof(FenceProof& proof) noexcept {
        if (proof.observing)
            return proof.status == FenceProofStatus::COMPLETE;
        const ProofObservation observing(proof.observing);
        if (!fence_proof_terminal(proof) && proof.origin && proof.origin->lost)
            observe_fence_proof(proof, vk::Result::eErrorDeviceLost);
        if ((proof.status == FenceProofStatus::PENDING || proof.status == FenceProofStatus::UNCERTAIN) && proof.fence)
            observe_fence_proof(proof, operation::poll_fence(proof.device, proof.fence));
        return proof.status == FenceProofStatus::COMPLETE;
    }
    bool wait_fence_proof(FenceProof& proof, std::uint64_t timeout) noexcept {
        if (proof.status == FenceProofStatus::UNCERTAIN)
            return poll_fence_proof(proof);
        if (proof.observing)
            return proof.status == FenceProofStatus::COMPLETE;
        const ProofObservation observing(proof.observing);
        if (!fence_proof_terminal(proof) && proof.origin && proof.origin->lost)
            observe_fence_proof(proof, vk::Result::eErrorDeviceLost);
        if ((proof.status == FenceProofStatus::PENDING || proof.status == FenceProofStatus::UNCERTAIN) && proof.fence)
            observe_fence_proof(proof, operation::wait_fence(proof.device, proof.fence, timeout));
        return proof.status == FenceProofStatus::COMPLETE;
    }
    PresentEnqueue classify_present_result(vk::Result result) noexcept {
        if (result == static_cast<vk::Result>(VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT))
            return PresentEnqueue::ENQUEUED;
        switch (result) {
            case vk::Result::eSuccess:
            case vk::Result::eSuboptimalKHR:
            case vk::Result::eErrorOutOfDateKHR:
            case vk::Result::eErrorSurfaceLostKHR:
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
                if (state.origin)
                    state.origin->lost = true;
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
        if (state.observing)
            return state.status == PresentStatus::COMPLETE;
        const ProofObservation observing(state.observing);
        if (!present_terminal(state) && state.origin && state.origin->lost)
            observe_present_proof(state, vk::Result::eErrorDeviceLost);
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
        if (state.status == PresentStatus::UNCERTAIN)
            return poll_present(state); // enqueue不明のFenceは無限待機しない。
        if (state.observing)
            return state.status == PresentStatus::COMPLETE;
        const ProofObservation observing(state.observing);
        if (!present_terminal(state) && state.origin && state.origin->lost)
            observe_present_proof(state, vk::Result::eErrorDeviceLost);
        if (state.status == PresentStatus::SUBMITTED && state.strict && state.proof)
            observe_present_proof(state, operation::wait_present_fence(state.device, state.proof, timeout));
        else if (state.status == PresentStatus::SUBMITTED && state.reacquire) {
            if (wait_fence_proof(*state.reacquire, timeout))
                state.status = PresentStatus::COMPLETE;
            else if (state.reacquire->status == FenceProofStatus::DEVICE_LOST)
                observe_present_proof(state, vk::Result::eErrorDeviceLost);
        }
        return state.status == PresentStatus::COMPLETE;
    }
} // namespace VSL_NAMESPACE::sync
