#pragma once
#include <VSL/define.hpp>
#include <vulkan/vulkan.hpp>
#include <cstdint>
#include <memory>

namespace VSL_NAMESPACE::sync {
    enum class PresentStatus {
        PENDING,
        SUBMITTED,
        COMPLETE,
        FAILED,
        CANCELLED,
        UNCERTAIN,
        LEGACY_RELEASED
    };
    enum class PresentEnqueue {
        ENQUEUED,
        NOT_ENQUEUED,
        DEVICE_LOST,
        UNKNOWN
    };
    enum class FenceProofStatus {
        UNARMED,
        NOT_ACQUIRED,
        PENDING,
        COMPLETE,
        DEVICE_LOST,
        UNCERTAIN
    };
    // native Fenceの所有者は、破棄／再利用より先にterminalを保存する。
    struct FenceProof {
        vk::Device device;
        vk::Fence fence;
        FenceProofStatus status = FenceProofStatus::UNARMED;
        vk::Result result = vk::Result::eNotReady;
    };
    // ownerは外側のlifetimeが保持する。terminal保存後は再利用済みFenceを観測しない。
    struct PresentState {
        PresentStatus status = PresentStatus::PENDING;
        vk::Result result = vk::Result::eNotReady;
        vk::Device device;
        vk::Fence proof;
        std::shared_ptr<FenceProof> reacquire;
        bool strict = false;
    };
    PresentEnqueue classify_present_result(vk::Result result) noexcept;
    bool fence_proof_terminal(const FenceProof& proof) noexcept;
    void save_fence_observation(FenceProof& proof, vk::Result result) noexcept;
    void save_present_observation(PresentState& state, vk::Result result) noexcept;
    bool poll_fence_proof(FenceProof& proof) noexcept;
    bool wait_fence_proof(FenceProof& proof, std::uint64_t timeout = UINT64_MAX) noexcept;
    PresentEnqueue commit_present(PresentState& state, vk::Result result) noexcept;
    bool present_terminal(const PresentState& state) noexcept;
    void fail_present(PresentState& state, vk::Result result) noexcept;
    bool poll_present(PresentState& state) noexcept;
    bool wait_present(PresentState& state, std::uint64_t timeout = UINT64_MAX) noexcept;
} // namespace VSL_NAMESPACE::sync
