#include <VSL/lifetime/acquire_ticket.hpp>
#include <VSL/sync/device_call.hpp>
#include <VSL/lifetime/swapchain_storage.hpp>
#include <VSL/lifetime/synchronization_use.hpp>
#include <VSL/lifetime/synchronization_access.hpp>
#include <VSL/lifetime/phase_call.hpp>
#include <VSL/lifetime/submission_retention.hpp>
#include <VSL/operation/presentation_ops.hpp>
#include <VSL/operation/submission_ops.hpp>
#include <VSL/sync/submission_completion.hpp>
#include <algorithm>
#include <mutex>
#include <new>
#include <stdexcept>
#include <utility>

namespace VSL_NAMESPACE::lifetime {
    namespace {
        struct AcquisitionQuarantine {
            std::mutex mutex;
            std::shared_ptr<AcquireTicket> head;
            size_t count = 0;
            bool collecting = false;
        };
        AcquisitionQuarantine& acquisition_quarantine() noexcept;
        void store_source(AcquireTicket& ticket, size_t source, const std::shared_ptr<void>& owner);
        bool acquisition_ready(const AcquireTicket& ticket) noexcept;
        bool acquisition_releasable(const AcquireTicket& ticket) noexcept;
        void require_recovery(impl::SwapchainImpl& generation) noexcept;
        bool observe_graphics(AcquireTicket& ticket, AcquisitionObservation observation) noexcept;
        bool observe_acquisition(AcquireTicket& ticket, bool wait) noexcept;
        void save_acquisition_loss(AcquireTicket& ticket) noexcept;
        bool drain_signal(AcquireTicket& ticket, impl::SemaphoreImpl& source, std::uint32_t index, bool wait) noexcept;
        bool matches_acquisition_scope(const AcquireTicket& ticket, const std::shared_ptr<impl::SurfaceImpl>& surface,
                                       std::span<const std::shared_ptr<SurfacePresentationDomain>> domains) noexcept;
        void prepend_acquisition_chain(std::shared_ptr<AcquireTicket>& head, std::shared_ptr<AcquireTicket>& chain) noexcept;

        AcquisitionQuarantine& acquisition_quarantine() noexcept {
            static_assert(std::is_nothrow_default_constructible_v<AcquisitionQuarantine>);
            alignas(AcquisitionQuarantine) static std::byte STORAGE[sizeof(AcquisitionQuarantine)];
            static auto* const REGISTRY = std::construct_at(reinterpret_cast<AcquisitionQuarantine*>(STORAGE));
            return *REGISTRY;
        }
        void store_source(AcquireTicket& ticket, size_t source, const std::shared_ptr<void>& owner) {
            if (!owner.use_count())
                return;
            for (std::uint8_t i = 0; i < ticket.owner_count; ++i)
                if (!owner.owner_before(ticket.owners[i]) && !ticket.owners[i].owner_before(owner)) {
                    ticket.source_owners[source] = i;
                    return;
                }
            if (ticket.owner_count == ticket.owners.size())
                throw std::logic_error("Acquisition owner capacity exceeded");
            ticket.source_owners[source] = ticket.owner_count;
            ticket.owners[ticket.owner_count++] = owner;
        }
        bool matches_acquisition_scope(const AcquireTicket& ticket, const std::shared_ptr<impl::SurfaceImpl>& surface,
                                       std::span<const std::shared_ptr<SurfacePresentationDomain>> domains) noexcept {
            if (!ticket.swapchain)
                return false;
            if (surface)
                return ticket.swapchain->surface == surface.get();
            for (const auto& domain : domains)
                if (const auto parent = domain->surface.lock(); parent && parent.get() == ticket.swapchain->surface)
                    return true;
            return false;
        }
        void prepend_acquisition_chain(std::shared_ptr<AcquireTicket>& head, std::shared_ptr<AcquireTicket>& chain) noexcept {
            if (!chain)
                return;
            auto* tail = chain.get();
            while (tail->quarantine_next)
                tail = tail->quarantine_next.get();
            tail->quarantine_next = std::move(head);
            head = std::move(chain);
        }
        bool acquisition_ready(const AcquireTicket& ticket) noexcept {
            using Status = sync::AcquisitionStatus;
            return ticket.state.status == Status::DELEGATED || ticket.state.status == Status::RETURNED ||
                   ticket.state.status == Status::ABANDONED || ticket.state.status == Status::DEVICE_LOST ||
                   ticket.state.status == Status::NOT_ACQUIRED;
        }
        bool acquisition_releasable(const AcquireTicket& ticket) noexcept {
            return acquisition_ready(ticket) && !ticket.state.native_call_active &&
                   (!ticket.graphics || !sync::pending_use(*ticket.graphics)) &&
                   (!ticket.evidence || (!ticket.evidence->proof.observing && sync::fence_proof_terminal(ticket.evidence->proof)));
        }
        void require_recovery(impl::SwapchainImpl& generation) noexcept {
            if (generation.state == SwapchainState::ACTIVE)
                generation.state = SwapchainState::RECOVERY_REQUIRED;
        }
        void save_acquisition_loss(AcquireTicket& ticket) noexcept {
            ticket.device->lost = true;
            if (acquisition_ready(ticket) && ticket.state.status != sync::AcquisitionStatus::DELEGATED)
                return;
            ticket.state.status = sync::AcquisitionStatus::DEVICE_LOST;
            ticket.state.drain.complete = true;
            ticket.state.drain.observation = static_cast<std::int32_t>(vk::Result::eErrorDeviceLost);
            if (ticket.evidence) {
                if (!sync::fence_proof_terminal(ticket.evidence->proof)) {
                    ticket.evidence->proof.result = vk::Result::eErrorDeviceLost;
                    ticket.evidence->proof.status = sync::FenceProofStatus::DEVICE_LOST;
                }
                ticket.evidence->finished = true;
            }
            if (ticket.available_use)
                ticket.available_use->complete = true;
            if (ticket.finished_use)
                ticket.finished_use->complete = true;
            if (ticket.record)
                sync::fail_present(ticket.record->state, vk::Result::eErrorDeviceLost);
        }
        bool observe_graphics(AcquireTicket& ticket, AcquisitionObservation observation) noexcept {
            const auto result = ticket.graphics;
            if (!result || result->native.active || result->status == Completion::Status::PENDING)
                return false;
            if (!sync::native_pending(*result))
                return true;
            const auto work = result->work.lock();
            if (!work)
                return false;
            if (observation.idle == vk::Result::eSuccess || observation.idle == vk::Result::eErrorDeviceLost) {
                sync::save_native_idle(*work, static_cast<std::int32_t>(observation.idle));
                return !sync::native_pending(*result);
            }
            if (observation.graphics_guarded && result->native.enqueue == sync::SubmissionEnqueue::UNKNOWN)
                return false;
            std::optional<SubmissionObservation> guard;
            if (!observation.graphics_guarded) {
                guard.emplace(*work);
                if (!guard->active())
                    return false;
            }
            auto native = ticket.device->lost ? vk::Result::eErrorDeviceLost
                                              : (result->native.enqueue == sync::SubmissionEnqueue::UNKNOWN
                                                     ? operation::wait_device_idle(ticket.device->device)
                                                     : (observation.wait ? operation::wait_fence(ticket.device->device, work->fence, UINT64_MAX)
                                                                         : operation::poll_fence(ticket.device->device, work->fence)));
            if (ticket.device->lost)
                native = vk::Result::eErrorDeviceLost;
            if (guard)
                guard->finish();
            sync::save_native_idle(*work, static_cast<std::int32_t>(native));
            return !sync::native_pending(*result);
        }
        bool observe_acquisition(AcquireTicket& ticket, bool wait) noexcept {
            if (!ticket.evidence)
                return !ticket.state.acquired;
            auto& proof = ticket.evidence->proof;
            if (ticket.device->lost) {
                save_acquisition_loss(ticket);
                return true;
            }
            if (!sync::fence_proof_terminal(proof)) {
                if (!wait || ticket.state.status == sync::AcquisitionStatus::UNCERTAIN)
                    sync::poll_fence_proof(proof);
                else
                    sync::wait_fence_proof(proof);
            }
            if (proof.status == sync::FenceProofStatus::DEVICE_LOST)
                save_acquisition_loss(ticket);
            return sync::fence_proof_terminal(proof);
        }
        bool drain_signal(AcquireTicket& ticket, impl::SemaphoreImpl& source, std::uint32_t index, bool wait) noexcept {
            if (source.use.accessing)
                return false;
            const NativeCall access(source.use.accessing);
            auto& drain = ticket.state.drain;
            if (drain.enqueue == sync::SubmissionEnqueue::NOT_CALLED || drain.enqueue == sync::SubmissionEnqueue::NOT_ENQUEUED) {
                const auto& marker = ticket.graphics->native.enqueue == sync::SubmissionEnqueue::ENQUEUED ? ticket.finished_use : ticket.available_use;
                if (!marker || marker->element != index)
                    return false;
                try {
                    sync::require_present_wait(source, index, *marker);
                } catch (...) {
                    return false;
                }
                const auto semaphore = source.semaphores[index];
                const auto stage = vk::PipelineStageFlags(vk::PipelineStageFlagBits::eAllCommands);
                const vk::SubmitInfo info{
                    .waitSemaphoreCount = 1,
                    .pWaitSemaphores = &semaphore,
                    .pWaitDstStageMask = &stage,
                };
                const auto result = operation::queue_submit(ticket.device->device, ticket.graphics_queue, info, {});
                sync::save_submit_result(drain, static_cast<std::int32_t>(result));
                if (drain.enqueue == sync::SubmissionEnqueue::UNKNOWN)
                    sync::poison_semaphore_element(source, index);
                if (drain.enqueue == sync::SubmissionEnqueue::DEVICE_LOST) {
                    save_acquisition_loss(ticket);
                    return true;
                }
                if (drain.enqueue == sync::SubmissionEnqueue::NOT_ENQUEUED)
                    return false;
            }
            if (!drain.complete) {
                if (!wait)
                    return false;
                auto idle = operation::wait_device_idle(ticket.device->device);
                if (ticket.device->lost)
                    idle = vk::Result::eErrorDeviceLost;
                if (idle == vk::Result::eErrorDeviceLost) {
                    save_acquisition_loss(ticket);
                    return true;
                }
                if (idle != vk::Result::eSuccess)
                    return false;
                drain.complete = true;
                drain.observation = static_cast<std::int32_t>(idle);
            }
            return true;
        }
    } // namespace

    std::shared_ptr<impl::SwapchainImpl> AcquireTicket::generation() const {
        return swapchain ? std::shared_ptr<impl::SwapchainImpl>(owners[source_owners[0]], swapchain) : nullptr;
    }
    std::shared_ptr<impl::SemaphoreImpl> AcquireTicket::available_owner() const {
        return available ? std::shared_ptr<impl::SemaphoreImpl>(owners[source_owners[1]], available) : nullptr;
    }
    std::shared_ptr<impl::SemaphoreImpl> AcquireTicket::finished_owner() const {
        return finished ? std::shared_ptr<impl::SemaphoreImpl>(owners[source_owners[2]], finished) : nullptr;
    }
    std::shared_ptr<impl::FenceImpl> AcquireTicket::acquire_fence_owner() const {
        return acquire_fences ? std::shared_ptr<impl::FenceImpl>(owners[source_owners[3]], acquire_fences) : nullptr;
    }
    void AcquireTicket::reject_acquisition() noexcept {
        if (state.acquired || state.status == sync::AcquisitionStatus::UNCERTAIN)
            return;
        state.status = sync::AcquisitionStatus::NOT_ACQUIRED;
        if (evidence) {
            evidence->proof.status = sync::FenceProofStatus::NOT_ACQUIRED;
            evidence->finished = true;
            if (acquire_fences)
                release_acquire_fence(*acquire_fences, acquire_slot, {evidence, &evidence->proof});
        }
        if (available_use)
            available_use->complete = true;
        if (finished_use)
            finished_use->complete = true;
    }
    AcquireTicket::~AcquireTicket() {
        if (!registered && !state.acquired)
            reject_acquisition();
    }
    std::shared_ptr<AcquireTicket> prepare_acquire_ticket(const std::shared_ptr<SurfacePresentationDomain>& domain,
                                                          const std::shared_ptr<impl::SwapchainImpl>& generation,
                                                          const std::shared_ptr<impl::SemaphoreImpl>& available,
                                                          const std::shared_ptr<impl::SemaphoreImpl>& finished,
                                                          const std::shared_ptr<impl::FenceImpl>& fences,
                                                          const std::shared_ptr<impl::SubmissionImpl>& graphics,
                                                          vk::Queue graphics_queue, vk::Queue present_queue, std::uint32_t slot) {
        require_swapchain_owner(generation);
        const auto surface = domain ? domain->surface.lock() : nullptr;
        if (!domain || domain->closing || !graphics || !sync::recordable(*graphics) || !fences ||
            !surface || surface.get() != generation->surface ||
            fences->device != generation->device || slot >= fences->fences.size())
            throw std::invalid_argument("Acquisition requires a recording result and a matching fence slot");
        for (const auto& previous : domain->tickets)
            if (previous->state.abandon_requested && !previous->state.native_call_active &&
                previous->state.status == sync::AcquisitionStatus::UNARMED)
                previous->reject_acquisition();
        detail::SynchronizationAccess::RequireCanonical(fences);
        if (available) {
            require_semaphore_owner(available);
            if (available->device != generation->device)
                throw std::invalid_argument("Acquisition semaphore Device mismatch");
            sync::require_acquire_signal(*available, slot);
        }
        if (finished) {
            require_semaphore_owner(finished);
            if (finished->device != generation->device || finished->semaphores.size() < generation->swap_chain_images.size())
                throw std::invalid_argument("Finished semaphore source must cover its generation images");
        }
        auto ticket = std::make_shared<AcquireTicket>();
        store_source(*ticket, 0, generation);
        store_source(*ticket, 1, available);
        store_source(*ticket, 2, finished);
        store_source(*ticket, 3, fences);
        ticket->device = generation->device;
        ticket->swapchain = generation.get();
        ticket->available = available.get();
        ticket->finished = finished.get();
        ticket->acquire_fences = fences.get();
        ticket->acquire_slot = slot;
        ticket->graphics_queue = graphics_queue;
        ticket->present_queue = present_queue;
        ticket->graphics = graphics;
        ticket->presentations = domain;
        ticket->evidence = prepare_acquire_evidence(*domain, generation, fences->fences[slot]);
        if (available) {
            ticket->available_use = sync::track_semaphore_wsi(*available, graphics, slot);
            ticket->available_use->lease = ticket->evidence;
        }
        if (finished) {
            ticket->finished_use = sync::track_semaphore_wsi(*finished);
            ticket->finished_use->acquisition = graphics;
            ticket->finished_use->lease = ticket->evidence;
        }
        domain->tickets.push_back(ticket);
        ticket->registered = true;
        return ticket;
    }
    void transfer_acquire_graphics(AcquireTicket& ticket, const std::shared_ptr<impl::SubmissionImpl>& graphics) {
        if (!graphics || !sync::recordable(*graphics) || ticket.state.abandon_requested || ticket.state.native_call_active || ticket.state.drain.enqueue != sync::SubmissionEnqueue::NOT_CALLED ||
            !ticket.graphics || ticket.graphics->native.active || ticket.graphics->native.observing || sync::native_pending(*ticket.graphics) ||
            (ticket.graphics->native.enqueue != sync::SubmissionEnqueue::NOT_CALLED && ticket.graphics->native.enqueue != sync::SubmissionEnqueue::NOT_ENQUEUED) ||
            ticket.state.status == sync::AcquisitionStatus::DELEGATED)
            throw std::logic_error("Acquisition graphics consumer cannot change during native use or recovery");
        if (!ticket.cancelled)
            ticket.cancelled = ticket.graphics;
        ticket.graphics = graphics;
        if (ticket.available_use)
            ticket.available_use->acquisition = graphics;
        if (ticket.finished_use)
            ticket.finished_use->acquisition = graphics;
    }
    void adopt_acquisition(AcquireTicket& ticket, vk::Result result, std::uint32_t image) noexcept {
        if (ticket.device->lost || result == vk::Result::eErrorDeviceLost) {
            save_acquisition_loss(ticket);
            return;
        }
        if (result == vk::Result::eSuccess || result == vk::Result::eSuboptimalKHR) {
            ticket.state.acquired = true;
            ticket.state.status = sync::AcquisitionStatus::ACQUIRED;
            ticket.evidence->proof.status = sync::FenceProofStatus::PENDING;
            ticket.evidence->finished = true;
            if (image < ticket.swapchain->swap_chain_images.size()) {
                ticket.acquired_index = image;
                if (ticket.finished_use) {
                    ticket.finished_use->element = image;
                    const auto previous = ticket.swapchain->presentation_context->images[image].record.lock();
                    if (previous)
                        ticket.finished_use->predecessor = previous->marker;
                }
                try {
                    commit_acquire_evidence(ticket.evidence, *ticket.swapchain, image);
                } catch (...) {
                    require_recovery(*ticket.swapchain);
                }
            } else
                require_recovery(*ticket.swapchain);
        } else if (result == vk::Result::eErrorDeviceLost)
            save_acquisition_loss(ticket);
        else if (result == vk::Result::eTimeout || result == vk::Result::eNotReady || result == vk::Result::eErrorOutOfDateKHR ||
                 result == vk::Result::eErrorSurfaceLostKHR || result == vk::Result::eErrorOutOfHostMemory || result == vk::Result::eErrorOutOfDeviceMemory ||
                 result == static_cast<vk::Result>(VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT))
            ticket.reject_acquisition();
        else {
            ticket.state.status = sync::AcquisitionStatus::UNCERTAIN;
            ticket.evidence->proof.status = sync::FenceProofStatus::UNCERTAIN;
            ticket.evidence->proof.result = result;
            ticket.evidence->finished = true;
            if (ticket.available)
                sync::poison_semaphore_element(*ticket.available, ticket.acquire_slot);
            require_recovery(*ticket.swapchain);
        }
    }
    void require_presentable_acquisition(const AcquireTicket& ticket) {
        if (ticket.released || ticket.state.abandon_requested || ticket.state.native_call_active ||
            ticket.state.drain.enqueue != sync::SubmissionEnqueue::NOT_CALLED || !ticket.state.acquired ||
            ticket.acquired_index == UINT32_MAX || !ticket.graphics || ticket.graphics->native.active ||
            ticket.graphics->native.enqueue != sync::SubmissionEnqueue::ENQUEUED)
            throw std::logic_error("Acquisition cannot enter presentation");
    }
    void delegate_acquisition(AcquireTicket& ticket, const std::shared_ptr<PresentRecord>& record) noexcept {
        ticket.record = record;
        if (record->state.status != sync::PresentStatus::PENDING && record->state.status != sync::PresentStatus::CANCELLED)
            ticket.state.status = sync::AcquisitionStatus::DELEGATED;
        if (record->state.status == sync::PresentStatus::UNCERTAIN)
            require_recovery(*ticket.swapchain);
    }
    void request_acquire_abandon(AcquireTicket& ticket) noexcept {
        if (ticket.state.status == sync::AcquisitionStatus::DELEGATED)
            return;
        ticket.state.abandon_requested = true;
        if (ticket.swapchain && ticket.state.acquired && ticket.state.status != sync::AcquisitionStatus::DELEGATED)
            require_recovery(*ticket.swapchain);
    }
    void recover_acquisition(AcquireTicket& ticket, AcquisitionObservation observation) noexcept {
        if (ticket.released || ticket.state.native_call_active || (ticket.graphics && ticket.graphics->native.active))
            return;
        if (acquisition_ready(ticket) && ticket.state.status != sync::AcquisitionStatus::DELEGATED)
            return;
        if (ticket.device && !ticket.device->lost && sync::NativeDeviceCall::Active(ticket.device->device))
            return;
        try {
            const NativeCall call(ticket.state.native_call_active);
            if (ticket.device->lost) {
                save_acquisition_loss(ticket);
                return;
            }
            if (!ticket.state.abandon_requested && ticket.state.status != sync::AcquisitionStatus::DELEGATED)
                return;
            if (ticket.state.status == sync::AcquisitionStatus::UNARMED)
                ticket.reject_acquisition();
            if (ticket.state.status == sync::AcquisitionStatus::NOT_ACQUIRED)
                return;
            if (!observe_acquisition(ticket, observation.wait) || !observe_graphics(ticket, observation))
                return;
            if (ticket.device->lost) {
                save_acquisition_loss(ticket);
                return;
            }
            if (ticket.state.status == sync::AcquisitionStatus::UNCERTAIN)
                return;
            if (ticket.graphics->native.enqueue == sync::SubmissionEnqueue::ENQUEUED && ticket.available_use)
                ticket.available_use->complete = true;
            if (ticket.state.status == sync::AcquisitionStatus::DELEGATED)
                return;
            require_recovery(*ticket.swapchain);
            if (ticket.graphics->native.enqueue == sync::SubmissionEnqueue::UNKNOWN) {
                if (ticket.available_use)
                    ticket.available_use->complete = true;
                if (ticket.finished_use)
                    ticket.finished_use->complete = true;
                if (ticket.record && ticket.record->state.status == sync::PresentStatus::PENDING)
                    ticket.record->state.status = sync::PresentStatus::CANCELLED;
                ticket.state.status = sync::AcquisitionStatus::ABANDONED;
                return;
            }
            const bool submitted = ticket.graphics->native.enqueue == sync::SubmissionEnqueue::ENQUEUED;
            auto* source = submitted ? ticket.finished : ticket.available;
            const auto index = submitted ? ticket.acquired_index : ticket.acquire_slot;
            if (source && !drain_signal(ticket, *source, index, observation.wait))
                return;
            if (ticket.device->lost)
                return;
            if (ticket.state.drain.enqueue == sync::SubmissionEnqueue::UNKNOWN) {
                if (ticket.available_use)
                    ticket.available_use->complete = true;
                if (ticket.finished_use)
                    ticket.finished_use->complete = true;
                if (ticket.record && ticket.record->state.status == sync::PresentStatus::PENDING)
                    ticket.record->state.status = sync::PresentStatus::CANCELLED;
                ticket.state.status = sync::AcquisitionStatus::ABANDONED;
                return;
            }
            if (ticket.available_use)
                ticket.available_use->complete = true;
            if (ticket.finished_use)
                ticket.finished_use->complete = true;
            if (ticket.record && ticket.record->state.status == sync::PresentStatus::PENDING)
                ticket.record->state.status = sync::PresentStatus::CANCELLED;
            if (!ticket.state.release_attempted && ticket.acquired_index != UINT32_MAX &&
                has_device_features(ticket.device->enabled_features, DeviceFeature::SWAPCHAIN_MAINTENANCE1)) {
                ticket.state.release_attempted = true;
                const auto result = operation::release_swapchain_image(ticket.device->device, ticket.swapchain->swap_chain, ticket.acquired_index);
                if (ticket.device->lost || result == vk::Result::eErrorDeviceLost) {
                    save_acquisition_loss(ticket);
                    return;
                }
                ticket.state.status = result == vk::Result::eSuccess ? sync::AcquisitionStatus::RETURNED : sync::AcquisitionStatus::ABANDONED;
            } else
                ticket.state.status = sync::AcquisitionStatus::ABANDONED;
        } catch (...) {
            // 証明や検証が未完なら予約とownerを残し、次の明示回収で再試行する。
        }
    }
    void release_acquire_owners(AcquireTicket& ticket) noexcept {
        if (ticket.released || !acquisition_ready(ticket) || ticket.state.native_call_active)
            return;
        if (ticket.graphics && sync::pending_use(*ticket.graphics))
            return;
        if (ticket.evidence && (ticket.evidence->proof.observing || !sync::fence_proof_terminal(ticket.evidence->proof)))
            return;
        if (ticket.acquire_fences && ticket.evidence && ticket.state.status != sync::AcquisitionStatus::DELEGATED)
            release_acquire_fence(*ticket.acquire_fences, ticket.acquire_slot, {ticket.evidence, &ticket.evidence->proof});
        if (ticket.evidence)
            ticket.evidence->proof.origin = nullptr;
        ticket.released = true;
        ticket.device = nullptr;
        ticket.swapchain = nullptr;
        ticket.available = ticket.finished = nullptr;
        ticket.acquire_fences = nullptr;
        ticket.source_owners.fill(AcquireTicket::NO_OWNER);
        ticket.record.reset();
        ticket.available_use.reset();
        ticket.finished_use.reset();
        ticket.owners = {};
    }
    void quarantine_acquisition(const std::shared_ptr<AcquireTicket>& ticket) noexcept {
        if (ticket->quarantined || ticket->released)
            return;
        auto& registry = acquisition_quarantine();
        const std::lock_guard lock(registry.mutex);
        ticket->quarantined = true;
        ticket->registered = false;
        ticket->quarantine_next = std::move(registry.head);
        registry.head = ticket;
        ++registry.count;
    }
    size_t quarantined_acquisition_count() noexcept {
        auto& registry = acquisition_quarantine();
        const std::lock_guard lock(registry.mutex);
        return registry.count;
    }
    AcquisitionSnapshot::AcquisitionSnapshot(std::shared_ptr<impl::LogicalDeviceImpl> device,
                                             std::span<const std::shared_ptr<SurfacePresentationDomain>> domains,
                                             std::shared_ptr<impl::SurfaceImpl> surface)
        : _device(std::move(device)), _surface(std::move(surface)), _domains(domains) {
        size_t count = 0;
        for (const auto& domain : domains) {
            if (domain->tickets.size() > std::numeric_limits<size_t>::max() - count)
                throw std::overflow_error("Acquisition snapshot capacity overflow");
            count += domain->tickets.size();
        }
        auto& registry = acquisition_quarantine();
        const std::lock_guard lock(registry.mutex);
        if (registry.collecting)
            throw std::logic_error("Acquisition quarantine is already being observed");
        if (registry.count > std::numeric_limits<size_t>::max() - count)
            throw std::overflow_error("Acquisition snapshot capacity overflow");
        count += registry.count;
        _entries.reserve(count);
        _work.reserve(count);
        _observations.reserve(count);
        for (const auto& domain : domains)
            for (const auto& ticket : domain->tickets)
                if (!ticket->released && ticket->device == _device.get() && matches_acquisition_scope(*ticket, _surface, _domains))
                    capture_ticket(ticket);
        for (auto ticket = registry.head; ticket; ticket = ticket->quarantine_next)
            if (!ticket->released && ticket->device == _device.get() && matches_acquisition_scope(*ticket, _surface, _domains))
                capture_ticket(ticket);
        registry.collecting = true;
        _collecting = true;
        auto pending = std::move(registry.head);
        while (pending) {
            auto ticket = std::move(pending);
            pending = std::move(ticket->quarantine_next);
            auto& head = ticket->device == _device.get() && matches_acquisition_scope(*ticket, _surface, _domains) ? _selected : _foreign;
            ticket->quarantine_next = std::move(head);
            head = std::move(ticket);
        }
    }
    void AcquisitionSnapshot::capture_ticket(const std::shared_ptr<AcquireTicket>& ticket) {
        if (std::ranges::any_of(_entries, [&](const auto& entry) { return entry.ticket == ticket; }))
            return;
        const auto graphics = ticket->graphics;
        _entries.push_back({ticket, graphics, graphics && (graphics->native.enqueue == sync::SubmissionEnqueue::ENQUEUED || graphics->native.enqueue == sync::SubmissionEnqueue::UNKNOWN)});
        if (const auto work = graphics ? graphics->work.lock() : nullptr;
            work && std::ranges::find(_work, work) == _work.end()) {
            _work.push_back(work);
            _observations.emplace_back(*work);
        }
    }
    AcquisitionSnapshot::~AcquisitionSnapshot() {
        if (_collecting) {
            auto& registry = acquisition_quarantine();
            const std::lock_guard lock(registry.mutex);
            prepend_acquisition_chain(registry.head, _selected);
            prepend_acquisition_chain(registry.head, _foreign);
            registry.collecting = false;
        }
        for (auto& observation : _observations)
            observation.finish();
        release();
    }
    void AcquisitionSnapshot::save_observations(vk::Result idle, bool wait, bool observe_graphics) noexcept {
        if (_device->lost)
            idle = vk::Result::eErrorDeviceLost;
        if (idle == vk::Result::eErrorDeviceLost) {
            for (const auto& entry : _entries)
                if (!entry.ticket->released)
                    save_acquisition_loss(*entry.ticket);
            for (auto* ticket = _foreign.get(); ticket; ticket = ticket->quarantine_next.get())
                if (ticket->device == _device.get())
                    save_acquisition_loss(*ticket);
            for (const auto& work : _work)
                if (!work->result->native.active)
                    sync::save_native_idle(*work, static_cast<std::int32_t>(idle));
            return;
        }
        for (const auto& entry : _entries) {
            auto& ticket = *entry.ticket;
            if (ticket.released || ticket.graphics != entry.graphics)
                continue;
            if (!entry.submitted && entry.graphics &&
                (entry.graphics->native.enqueue == sync::SubmissionEnqueue::ENQUEUED || entry.graphics->native.enqueue == sync::SubmissionEnqueue::UNKNOWN))
                continue;
            if (!observe_graphics && !ticket.state.abandon_requested && entry.graphics && sync::native_pending(*entry.graphics)) {
                observe_acquisition(ticket, false);
                continue;
            }
            const bool owned_observation = entry.graphics && std::ranges::any_of(_observations, [&](const auto& observation) {
                                               return observation.owns(*entry.graphics);
                                           });
            if (entry.submitted && entry.graphics && !entry.graphics->native.active)
                if (const auto work = entry.graphics->work.lock(); work && sync::native_pending(*entry.graphics)) {
                    if (idle != vk::Result::eSuccess && !owned_observation)
                        continue;
                    const auto observed = idle == vk::Result::eSuccess ? idle : operation::poll_fence(_device->device, work->fence);
                    sync::save_native_idle(*work, static_cast<std::int32_t>(observed));
                }
            if (_device->lost) {
                save_observations(vk::Result::eErrorDeviceLost, false);
                return;
            }
            recover_acquisition(ticket, {entry.submitted ? idle : vk::Result::eNotReady, wait, true});
            if (_device->lost) {
                save_observations(vk::Result::eErrorDeviceLost, false);
                return;
            }
        }
    }
    void AcquisitionSnapshot::bookkeep() noexcept {
        if (!_collecting)
            return;
        for (auto& observation : _observations)
            observation.finish();
        for (const auto& entry : _entries)
            if (!entry.ticket->released && !acquisition_releasable(*entry.ticket))
                _pending = true;
        for (const auto& domain : _domains)
            std::erase_if(domain->tickets, [&](const auto& ticket) {
                const bool captured = std::ranges::any_of(_entries, [&](const auto& entry) { return entry.ticket == ticket; });
                if (captured && acquisition_releasable(*ticket)) {
                    ticket->registered = false;
                    return true;
                }
                return false;
            });
        std::shared_ptr<AcquireTicket> pending;
        while (_selected) {
            auto ticket = std::move(_selected);
            _selected = std::move(ticket->quarantine_next);
            auto& head = acquisition_releasable(*ticket) ? _ready : pending;
            ticket->quarantine_next = std::move(head);
            head = std::move(ticket);
        }
        auto& registry = acquisition_quarantine();
        const std::lock_guard lock(registry.mutex);
        prepend_acquisition_chain(registry.head, pending);
        prepend_acquisition_chain(registry.head, _foreign);
        for (auto* ticket = _ready.get(); ticket; ticket = ticket->quarantine_next.get()) {
            ticket->quarantined = false;
            --registry.count;
        }
        registry.collecting = false;
        _collecting = false;
    }
    void AcquisitionSnapshot::release() noexcept {
        if (!_collecting) {
            for (const auto& work : _work)
                if (sync::terminal_status(work->result->status) && !sync::native_pending(*work->result))
                    sync::finish_submission(*work);
            for (const auto& entry : _entries)
                release_acquire_owners(*entry.ticket);
        }
        while (_ready) {
            auto ticket = std::move(_ready);
            _ready = std::move(ticket->quarantine_next);
        }
        _observations.clear();
        _work.clear();
    }
    bool AcquisitionSnapshot::pending() const noexcept {
        if (!_collecting)
            return _pending;
        return std::ranges::any_of(_entries, [](const auto& entry) {
            return !entry.ticket->released && !acquisition_releasable(*entry.ticket);
        });
    }
    bool AcquisitionSnapshot::native_active() const noexcept {
        return std::ranges::any_of(_entries, [&](const auto& entry) {
            const bool external_observer = entry.graphics && entry.graphics->native.observing &&
                                           !std::ranges::any_of(_observations, [&](const auto& observation) { return observation.owns(*entry.graphics); });
            return entry.ticket->state.native_call_active || (entry.graphics && entry.graphics->native.active) ||
                   (entry.ticket->evidence && entry.ticket->evidence->proof.observing) || external_observer;
        });
    }
    std::vector<std::shared_ptr<AcquireTicket>> quarantine_acquisitions(SurfacePresentationDomain& domain) noexcept {
        auto pending = std::move(domain.tickets);
        for (const auto& ticket : pending) {
            request_acquire_abandon(*ticket);
            recover_acquisition(*ticket);
            if (!acquisition_ready(*ticket) || (ticket->graphics && sync::pending_use(*ticket->graphics)) ||
                (ticket->evidence && !sync::fence_proof_terminal(ticket->evidence->proof)))
                quarantine_acquisition(ticket);
            else
                ticket->registered = false;
        }
        return pending;
    }
} // namespace VSL_NAMESPACE::lifetime
