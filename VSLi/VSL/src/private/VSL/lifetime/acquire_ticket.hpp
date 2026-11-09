#pragma once
#include <VSL/lifetime/presentation_retention.hpp>
#include <VSL/sync/acquisition_state.hpp>
#include <VSL/lifetime/submission_retention.hpp>
#include <array>

namespace VSL_NAMESPACE::lifetime {
    struct AcquisitionObservation {
        vk::Result idle = vk::Result::eNotReady;
        bool wait = true, graphics_guarded = false;
    };
    struct AcquireTicket {
        static constexpr std::uint8_t NO_OWNER = UINT8_MAX;
        std::array<std::shared_ptr<void>, 4> owners;
        std::array<std::uint8_t, 4> source_owners{NO_OWNER, NO_OWNER, NO_OWNER, NO_OWNER};
        std::uint8_t owner_count = 0;
        impl::LogicalDeviceImpl* device = nullptr;
        impl::SwapchainImpl* swapchain = nullptr;
        impl::SemaphoreImpl* available = nullptr;
        impl::SemaphoreImpl* finished = nullptr;
        impl::FenceImpl* acquire_fences = nullptr;
        std::weak_ptr<SurfacePresentationDomain> presentations;
        std::shared_ptr<AcquireEvidence> evidence;
        std::shared_ptr<sync::SemaphoreWSIUse> available_use, finished_use;
        std::shared_ptr<impl::SubmissionImpl> graphics, cancelled;
        std::shared_ptr<PresentRecord> record;
        vk::Queue graphics_queue, present_queue;
        std::uint32_t acquire_slot = 0, acquired_index = UINT32_MAX;
        sync::AcquisitionState state;
        std::shared_ptr<AcquireTicket> quarantine_next;
        bool registered = false, quarantined = false, released = false;

        std::shared_ptr<impl::SwapchainImpl> generation() const;
        std::shared_ptr<impl::SemaphoreImpl> available_owner() const;
        std::shared_ptr<impl::SemaphoreImpl> finished_owner() const;
        std::shared_ptr<impl::FenceImpl> acquire_fence_owner() const;
        void reject_acquisition() noexcept;
        ~AcquireTicket();
    };
    class AcquisitionSnapshot {
        struct Entry {
            std::shared_ptr<AcquireTicket> ticket;
            std::shared_ptr<impl::SubmissionImpl> graphics;
            bool submitted = false;
        };
        std::shared_ptr<impl::LogicalDeviceImpl> _device;
        std::shared_ptr<impl::SurfaceImpl> _surface;
        std::span<const std::shared_ptr<SurfacePresentationDomain>> _domains;
        std::vector<Entry> _entries;
        std::vector<std::shared_ptr<impl::SubmissionWork>> _work;
        std::vector<SubmissionObservation> _observations;
        std::shared_ptr<AcquireTicket> _selected, _foreign, _ready;
        bool _collecting = false, _pending = false;
        void capture_ticket(const std::shared_ptr<AcquireTicket>& ticket);

    public:
        AcquisitionSnapshot(std::shared_ptr<impl::LogicalDeviceImpl> device,
                            std::span<const std::shared_ptr<SurfacePresentationDomain>> domains,
                            std::shared_ptr<impl::SurfaceImpl> surface = {});
        ~AcquisitionSnapshot();
        AcquisitionSnapshot(const AcquisitionSnapshot&) = delete;
        AcquisitionSnapshot& operator=(const AcquisitionSnapshot&) = delete;
        void save_observations(vk::Result idle, bool wait, bool observe_graphics = true) noexcept;
        void bookkeep() noexcept;
        void release() noexcept;
        bool pending() const noexcept;
        bool captured() const noexcept { return !_entries.empty(); }
        bool native_active() const noexcept;
    };
    std::shared_ptr<AcquireTicket> prepare_acquire_ticket(const std::shared_ptr<SurfacePresentationDomain>& domain,
                                                          const std::shared_ptr<impl::SwapchainImpl>& generation,
                                                          const std::shared_ptr<impl::SemaphoreImpl>& available,
                                                          const std::shared_ptr<impl::SemaphoreImpl>& finished,
                                                          const std::shared_ptr<impl::FenceImpl>& fences,
                                                          const std::shared_ptr<impl::SubmissionImpl>& graphics,
                                                          vk::Queue graphics_queue, vk::Queue present_queue, std::uint32_t slot);
    void transfer_acquire_graphics(AcquireTicket& ticket, const std::shared_ptr<impl::SubmissionImpl>& graphics);
    void adopt_acquisition(AcquireTicket& ticket, vk::Result result, std::uint32_t image) noexcept;
    void require_presentable_acquisition(const AcquireTicket& ticket);
    void delegate_acquisition(AcquireTicket& ticket, const std::shared_ptr<PresentRecord>& record) noexcept;
    void request_acquire_abandon(AcquireTicket& ticket) noexcept;
    void recover_acquisition(AcquireTicket& ticket, AcquisitionObservation observation = {}) noexcept;
    void release_acquire_owners(AcquireTicket& ticket) noexcept;
    void quarantine_acquisition(const std::shared_ptr<AcquireTicket>& ticket) noexcept;
    size_t quarantined_acquisition_count() noexcept;
    std::vector<std::shared_ptr<AcquireTicket>> quarantine_acquisitions(SurfacePresentationDomain& domain) noexcept;
} // namespace VSL_NAMESPACE::lifetime
