#pragma once
#include <VSL/vulkan/_pimpls.hpp>
#include <VSL/sync/presentation_state.hpp>
#include <span>

namespace VSL_NAMESPACE::lifetime {
    struct AcquireTicket;
    struct PresentRecord;
    struct ImagePresentSlot {
        std::weak_ptr<PresentRecord> record;
        std::uint64_t epoch = 0;
        std::uint64_t present_serial = 0;
        vk::Device device;
        vk::Queue queue;
        bool accessing = false;
    };
    // 世代内は借用と弱参照だけ。レコードの強い保持は外部Domainへ置く。
    struct SwapchainPresentationContext {
        impl::SwapchainImpl* generation = nullptr;
        impl::FenceImpl* fences = nullptr;
        impl::SemaphoreImpl* finished = nullptr;
        std::vector<ImagePresentSlot> images;
        explicit SwapchainPresentationContext(size_t count) : images(count) {}
    };
    struct PresentRecord {
        sync::PresentState state;
        std::shared_ptr<impl::SwapchainImpl> generation;
        std::shared_ptr<impl::SemaphoreImpl> semaphore;
        impl::SemaphoreImpl* semaphore_source = nullptr;
        std::shared_ptr<sync::SemaphoreWSIUse> marker;
        vk::Queue queue;
        vk::Semaphore wait_semaphore;
        std::uint32_t image = UINT32_MAX;
        std::uint64_t epoch = 0, serial = 0;
        bool released = false;
        std::shared_ptr<PresentRecord> quarantine_next;
        bool quarantined = false;
    };
    struct AcquireEvidence {
        sync::FenceProof proof;
        std::weak_ptr<impl::SwapchainImpl> generation;
        impl::SwapchainImpl* generation_address = nullptr;
        vk::Queue queue;
        std::uint64_t present_serial = 0;
        std::uint32_t image = UINT32_MAX;
        bool finished = false;
    };
    struct SurfacePresentationDomain {
        std::weak_ptr<impl::SurfaceImpl> surface;
        std::vector<std::shared_ptr<PresentRecord>> records;
        std::vector<std::shared_ptr<AcquireEvidence>> acquisitions;
        std::vector<std::shared_ptr<AcquireTicket>> tickets;
        bool closing = false, recovering = false;
        ~SurfacePresentationDomain();
    };
    struct PresentationSnapshot {
        std::shared_ptr<SurfacePresentationDomain> domain;
        std::vector<std::shared_ptr<PresentRecord>> records;
        std::vector<std::shared_ptr<AcquireEvidence>> acquisitions;
    };
    class QuarantinedPresentations {
        std::shared_ptr<impl::LogicalDeviceImpl> _device;
        std::shared_ptr<impl::SurfaceImpl> _surface;
        std::span<const std::shared_ptr<SurfacePresentationDomain>> _domains;
        std::shared_ptr<PresentRecord> _selected, _foreign, _ready;
        bool _collecting = false, _pending = false;

    public:
        QuarantinedPresentations(std::shared_ptr<impl::LogicalDeviceImpl> device,
                                 std::span<const std::shared_ptr<SurfacePresentationDomain>> domains,
                                 std::shared_ptr<impl::SurfaceImpl> surface = {});
        ~QuarantinedPresentations();
        QuarantinedPresentations(const QuarantinedPresentations&) = delete;
        QuarantinedPresentations& operator=(const QuarantinedPresentations&) = delete;
        void save_observations(vk::Result idle, bool wait) noexcept;
        void bookkeep() noexcept;
        void release() noexcept;
        bool pending() const noexcept;
    };
    std::shared_ptr<SurfacePresentationDomain> presentation_domain(const std::shared_ptr<impl::SwapchainImpl>& generation);
    void register_presentation_domain(CommandDomain& commands, const std::shared_ptr<SurfacePresentationDomain>& domain);
    std::shared_ptr<AcquireEvidence> prepare_acquire_evidence(SurfacePresentationDomain& domain, const std::shared_ptr<impl::SwapchainImpl>& generation, vk::Fence fence);
    void commit_acquire_evidence(const std::shared_ptr<AcquireEvidence>& evidence, impl::SwapchainImpl& generation, std::uint32_t image);
    std::shared_ptr<PresentRecord> prepare_present_record(SurfacePresentationDomain& domain, std::shared_ptr<impl::SwapchainImpl> generation,
                                                          std::shared_ptr<impl::SemaphoreImpl> semaphore, std::shared_ptr<sync::SemaphoreWSIUse> marker,
                                                          vk::Queue queue, std::uint32_t image);
    sync::PresentEnqueue enqueue_present_record(PresentRecord& record);
    void collect_presentations(std::shared_ptr<SurfacePresentationDomain> domain) noexcept;
    void collect_quarantined_presentations(const std::weak_ptr<impl::SurfaceImpl>& surface, vk::Device device,
                                           vk::Result known_idle = vk::Result::eNotReady) noexcept;
    size_t quarantined_presentation_count() noexcept;
    PresentationSnapshot snapshot_presentations(const std::shared_ptr<SurfacePresentationDomain>& domain, vk::Device device);
    void finish_presentations(PresentationSnapshot& snapshot, vk::Result idle_result);
    void observe_presentations(PresentationSnapshot& snapshot, vk::Result idle_result, bool wait) noexcept;
    void bookkeep_presentations(PresentationSnapshot& snapshot) noexcept;
    void release_presentations(PresentationSnapshot& snapshot) noexcept;
} // namespace VSL_NAMESPACE::lifetime
