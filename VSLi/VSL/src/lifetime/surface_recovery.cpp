#include <VSL/lifetime/surface_recovery.hpp>
#include <VSL/lifetime/recovery_batch.hpp>
#include <VSL/lifetime/acquire_ticket.hpp>
#include <VSL/lifetime/command_domain.hpp>
#include <VSL/lifetime/device_owner.hpp>
#include <VSL/lifetime/surface_storage.hpp>
#include <VSL/operation/submission_ops.hpp>
#include <VSL/sync/submission_completion.hpp>
#include <VSL/vulkan/surface.hpp>
#include <algorithm>
#include <stdexcept>

namespace VSL_NAMESPACE::lifetime {
    namespace {
        struct RecoveryResult {
            SurfaceRecoveryStatus status;
            bool presentation_pending = false;
        };
        struct RecoveryDomains {
            std::span<const std::shared_ptr<SurfacePresentationDomain>> domains;
            explicit RecoveryDomains(std::span<const std::shared_ptr<SurfacePresentationDomain>> domains);
            ~RecoveryDomains();
            void finish() noexcept;
        };
        RecoveryResult recover_batch(const std::shared_ptr<impl::LogicalDeviceImpl>& device,
                                     const std::shared_ptr<impl::SurfaceImpl>& surface,
                                     const std::shared_ptr<impl::CommandBufferImpl>& ring,
                                     std::span<const std::shared_ptr<SurfacePresentationDomain>> domains,
                                     std::span<const std::shared_ptr<impl::SubmissionWork>> known,
                                     bool wait, bool observe_graphics);
        SurfaceRecoveryStatus recover_surface_scope(LogicalDeviceAccessor device, const Surface& surface, SurfaceRecoveryMode mode, bool observe_graphics);
        RecoveryDomains::RecoveryDomains(std::span<const std::shared_ptr<SurfacePresentationDomain>> domains) : domains(domains) {
            for (const auto& domain : domains)
                domain->recovering = true;
        }
        RecoveryDomains::~RecoveryDomains() {
            finish();
        }
        void RecoveryDomains::finish() noexcept {
            for (const auto& domain : domains)
                domain->recovering = false;
            domains = {};
        }
        RecoveryResult recover_batch(const std::shared_ptr<impl::LogicalDeviceImpl>& device,
                                     const std::shared_ptr<impl::SurfaceImpl>& surface,
                                     const std::shared_ptr<impl::CommandBufferImpl>& ring,
                                     std::span<const std::shared_ptr<SurfacePresentationDomain>> domains,
                                     std::span<const std::shared_ptr<impl::SubmissionWork>> known,
                                     bool wait, bool observe_graphics) {
            if (std::ranges::any_of(domains, [](const auto& domain) { return domain->recovering; }))
                return {SurfaceRecoveryStatus::PENDING, true};
            RecoveryDomains recovering(domains);
            std::vector<PresentationSnapshot> presentation;
            presentation.reserve(domains.size());
            for (const auto& domain : domains) {
                PresentationSnapshot snapshot{domain};
                for (const auto& record : domain->records)
                    if (!record->released && record->state.device == device->device)
                        snapshot.records.push_back(record);
                for (const auto& evidence : domain->acquisitions)
                    if (evidence->proof.device == device->device)
                        snapshot.acquisitions.push_back(evidence);
                presentation.push_back(std::move(snapshot));
            }
            AcquisitionSnapshot acquisitions(device, domains, surface);
            const bool observing_present = std::ranges::any_of(presentation, [](const auto& snapshot) {
                return std::ranges::any_of(snapshot.records, [](const auto& record) { return record->state.observing; });
            });
            if (!device->lost && (acquisitions.native_active() || observing_present))
                return {SurfaceRecoveryStatus::PENDING, true};
            std::optional<SubmissionSnapshot> work;
            if (ring)
                work.emplace(ring, domains);
            else
                work.emplace(device, surface);
            std::vector<SubmissionObservation> observations;
            observations.reserve(known.size());
            for (const auto& item : known)
                observations.emplace_back(*item);
            QuarantinedPresentations orphan_present(device, domains, surface);
            auto idle = device->lost ? vk::Result::eErrorDeviceLost
                                     : (wait ? operation::wait_device_idle(device->device) : vk::Result::eNotReady);
            if (device->lost)
                idle = vk::Result::eErrorDeviceLost;
            if (wait && idle != vk::Result::eSuccess && idle != vk::Result::eErrorDeviceLost)
                vk::detail::throwResultException(idle, "Surface recovery could not observe Device idle");
            if (observe_graphics || idle == vk::Result::eErrorDeviceLost)
                work->save_observations(static_cast<std::int32_t>(idle));
            for (const auto& item : known)
                if (!item->result->native.active)
                    sync::save_native_idle(*item, static_cast<std::int32_t>(idle));
            acquisitions.save_observations(idle, wait, observe_graphics);
            for (auto& snapshot : presentation)
                observe_presentations(snapshot, device->lost ? vk::Result::eErrorDeviceLost : idle, wait);
            orphan_present.save_observations(device->lost ? vk::Result::eErrorDeviceLost : idle, wait);
            if (device->lost) {
                work->save_observations(static_cast<std::int32_t>(vk::Result::eErrorDeviceLost));
                for (const auto& item : known)
                    if (!item->result->native.active)
                        sync::save_native_idle(*item, static_cast<std::int32_t>(vk::Result::eErrorDeviceLost));
                acquisitions.save_observations(vk::Result::eErrorDeviceLost, false);
                for (auto& snapshot : presentation)
                    observe_presentations(snapshot, vk::Result::eErrorDeviceLost, false);
                orphan_present.save_observations(vk::Result::eErrorDeviceLost, false);
            }
            for (auto& observation : observations)
                observation.finish();
            work->bookkeep();
            acquisitions.bookkeep();
            orphan_present.bookkeep();
            for (auto& snapshot : presentation)
                bookkeep_presentations(snapshot);
            bool pending = work->pending() || acquisitions.pending() || orphan_present.pending();
            bool presentation_pending = orphan_present.pending();
            for (const auto& snapshot : presentation)
                for (const auto& record : snapshot.records)
                    if (!record->released) {
                        pending = true;
                        if (record->state.status != sync::PresentStatus::PENDING)
                            presentation_pending = true;
                    }
            recovering.finish();
            work->release();
            for (const auto& item : known)
                if (sync::terminal_status(item->result->status) && !sync::native_pending(*item->result))
                    sync::finish_submission(*item);
            acquisitions.release();
            for (auto& snapshot : presentation)
                release_presentations(snapshot);
            orphan_present.release();
            return {device->lost ? SurfaceRecoveryStatus::DEVICE_LOST : (pending ? SurfaceRecoveryStatus::PENDING : SurfaceRecoveryStatus::COMPLETE),
                    presentation_pending};
        }
        SurfaceRecoveryStatus recover_surface_scope(LogicalDeviceAccessor device, const Surface& surface, SurfaceRecoveryMode mode, bool observe_graphics) {
            require_device_owner(device);
            (void)surface.owner_ref();
            const auto native_device = device.data_;
            const auto parent = surface.data_;
            if (!parent || !parent->surface || !parent->owner_context || native_device->parent_device->vulkan != parent->vulkan)
                throw std::invalid_argument("Surface recovery requires a Surface on the Device Instance");
            if (mode != SurfaceRecoveryMode::POLL && mode != SurfaceRecoveryMode::WAIT)
                throw std::invalid_argument("Unknown Surface recovery mode");
            std::vector<std::shared_ptr<SurfacePresentationDomain>> domains;
            if (const auto domain = parent->owner_context->presentations.lock()) {
                if (domain->closing || parent->owner_context->presentation_identity != domain.get())
                    throw std::logic_error("Surface presentation domain is closing");
                domains.push_back(domain);
            } else if (parent->owner_context->presentation_identity)
                throw std::logic_error("Surface presentation domain is still retiring");
            return recover_batch(native_device, parent, {}, domains, {}, mode == SurfaceRecoveryMode::WAIT, observe_graphics).status;
        }
    } // namespace
    SurfaceRecoveryStatus recover_surface(LogicalDeviceAccessor device, const Surface& surface, SurfaceRecoveryMode mode) {
        return recover_surface_scope(device, surface, mode, true);
    }
    void recover_command_domain(CommandDomain& domain) {
        const auto buffer = domain.buffer;
        const auto device = device_from_impl(*buffer->command_pool->device).data_;
        const auto domains = domain.presentation_domains;
        std::vector<std::shared_ptr<impl::SubmissionWork>> work;
        work.reserve(domain.submissions.size());
        for (const auto& item : domain.submissions)
            if (item && (item->result->status == Completion::Status::SUBMITTED || sync::native_pending(*item->result)))
                work.push_back(item);
        const auto result = recover_batch(device, {}, buffer, domains, work, true, true);
        if (result.status == SurfaceRecoveryStatus::DEVICE_LOST)
            vk::detail::throwResultException(vk::Result::eErrorDeviceLost, "Device lost while waiting idle");
        if (result.presentation_pending)
            throw std::runtime_error("Presentation completion could not be established");
    }
    void collect_acquire_scope(AcquireTicket& ticket, bool wait) noexcept {
        if (ticket.released || ticket.state.native_call_active || !ticket.device || !ticket.swapchain ||
            (ticket.graphics && (ticket.graphics->native.active || ticket.graphics->native.observing)))
            return;
        try {
            const auto device = device_from_impl(*ticket.device);
            auto* const surface = ticket.swapchain->surface;
            if (!surface || !surface->owner_node)
                return;
            const Surface parent(surface->owner_node->owner_ref().alias(surface).share());
            (void)recover_surface_scope(device, parent, wait ? SurfaceRecoveryMode::WAIT : SurfaceRecoveryMode::POLL, wait);
        } catch (...) {
            // 保留資源はDomain／退避一覧へ残し、次の回収で再試行する。
        }
    }
} // namespace VSL_NAMESPACE::lifetime
