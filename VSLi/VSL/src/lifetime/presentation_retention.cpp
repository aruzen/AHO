#include <VSL/lifetime/presentation_retention.hpp>
#include <VSL/sync/device_call.hpp>
#include <VSL/lifetime/command_domain.hpp>
#include <VSL/lifetime/acquire_ticket.hpp>
#include <VSL/lifetime/phase_call.hpp>
#include <VSL/lifetime/surface_storage.hpp>
#include <VSL/lifetime/swapchain_storage.hpp>
#include <VSL/lifetime/synchronization_use.hpp>
#include <VSL/operation/presentation_ops.hpp>
#include <VSL/operation/synchronization_ops.hpp>
#include <VSL/operation/submission_ops.hpp>
#include <algorithm>
#include <atomic>
#include <limits>
#include <mutex>
#include <new>
#include <thread>

namespace VSL_NAMESPACE::lifetime {
    namespace {
        std::atomic<std::uint64_t> PRESENT_SERIAL{0};
        struct ImagePresentAccess {
            ImagePresentSlot& slot;
            explicit ImagePresentAccess(ImagePresentSlot& slot);
            ~ImagePresentAccess() { slot.accessing = false; }
        };
        struct ReleasedPresentOwners {
            std::shared_ptr<impl::SwapchainImpl> generation;
            std::shared_ptr<impl::SemaphoreImpl> semaphore;
            std::shared_ptr<sync::SemaphoreWSIUse> marker;
        };
        struct PresentationQuarantine {
            std::mutex mutex;
            std::shared_ptr<PresentRecord> head;
            size_t count = 0;
            bool collecting = false;
        };
        PresentationQuarantine& presentation_quarantine() noexcept;
        void quarantine_presentation(std::shared_ptr<PresentRecord> record) noexcept;
        std::uint64_t next_present_serial();
        bool same_generation(const AcquireEvidence& evidence, const std::shared_ptr<impl::SwapchainImpl>& generation) noexcept;
        void apply_acquisitions(std::span<const std::shared_ptr<PresentRecord>> records, std::span<const std::shared_ptr<AcquireEvidence>> acquisitions) noexcept;
        ReleasedPresentOwners detach_present_owners(PresentRecord& record) noexcept;
        void retire_presentations(PresentationSnapshot& snapshot);
        void observe_present_record(PresentRecord& record, vk::Result idle, bool wait) noexcept;
        bool matches_presentation_scope(const PresentRecord& record, const std::shared_ptr<impl::SurfaceImpl>& surface,
                                        std::span<const std::shared_ptr<SurfacePresentationDomain>> domains) noexcept;
        void prepend_present_chain(std::shared_ptr<PresentRecord>& head, std::shared_ptr<PresentRecord>& chain) noexcept;
        bool present_native_active(const PresentRecord& record) noexcept;

        PresentationQuarantine& presentation_quarantine() noexcept {
            static_assert(std::is_nothrow_default_constructible_v<PresentationQuarantine>);
            alignas(PresentationQuarantine) static std::byte STORAGE[sizeof(PresentationQuarantine)];
            // 終了時にも、証明のないnative資源を自動破棄しない。退避に追加確保は要らない。
            static auto* const REGISTRY = std::construct_at(reinterpret_cast<PresentationQuarantine*>(STORAGE));
            return *REGISTRY;
        }
        void quarantine_presentation(std::shared_ptr<PresentRecord> record) noexcept {
            auto& registry = presentation_quarantine();
            const std::lock_guard lock(registry.mutex);
            if (record->quarantined)
                return;
            record->quarantined = true;
            record->quarantine_next = std::move(registry.head);
            registry.head = std::move(record);
            ++registry.count;
        }
        ImagePresentAccess::ImagePresentAccess(ImagePresentSlot& slot) : slot(slot) {
            if (slot.accessing)
                throw std::logic_error("Swapchain image presentation is already being changed");
            slot.accessing = true;
        }
        std::uint64_t next_present_serial() {
            auto current = PRESENT_SERIAL.load(std::memory_order_relaxed);
            for (;;) {
                if (current == std::numeric_limits<std::uint64_t>::max())
                    throw std::overflow_error("Presentation serial overflow");
                if (PRESENT_SERIAL.compare_exchange_weak(current, current + 1, std::memory_order_relaxed))
                    return current + 1;
            }
        }
        bool same_generation(const AcquireEvidence& evidence, const std::shared_ptr<impl::SwapchainImpl>& generation) noexcept {
            return evidence.generation_address == generation.get() &&
                   !evidence.generation.owner_before(generation) && !generation.owner_before(evidence.generation);
        }
        void apply_acquisitions(std::span<const std::shared_ptr<PresentRecord>> records, std::span<const std::shared_ptr<AcquireEvidence>> acquisitions) noexcept {
            for (const auto& evidence : acquisitions) {
                if (!evidence->finished || !sync::poll_fence_proof(evidence->proof) || !evidence->present_serial)
                    continue;
                for (const auto& record : records) {
                    if (record->released || record->state.strict || record->state.status != sync::PresentStatus::SUBMITTED ||
                        !record->generation || record->state.device != evidence->proof.device)
                        continue;
                    const bool same = same_generation(*evidence, record->generation);
                    const bool returned_image = same && record->image == evidence->image && record->serial <= evidence->present_serial;
                    const bool retired_generation = !same && record->generation->state == SwapchainState::RETIRED &&
                                                    record->queue == evidence->queue && record->serial < evidence->present_serial;
                    if (returned_image || retired_generation)
                        record->state.status = sync::PresentStatus::COMPLETE;
                }
            }
        }
        ReleasedPresentOwners detach_present_owners(PresentRecord& record) noexcept {
            record.semaphore_source = nullptr;
            record.state.origin = nullptr;
            record.released = true;
            ReleasedPresentOwners owners;
            owners.generation.swap(record.generation);
            owners.semaphore.swap(record.semaphore);
            owners.marker.swap(record.marker);
            if (owners.marker && record.state.status != sync::PresentStatus::CANCELLED)
                owners.marker->complete = true;
            return owners;
        }
        void retire_presentations(PresentationSnapshot& snapshot) {
            if (snapshot.domain->recovering)
                return;
            bookkeep_presentations(snapshot);
            release_presentations(snapshot);
        }
        void observe_present_record(PresentRecord& record, vk::Result idle, bool wait) noexcept {
            if (idle == vk::Result::eErrorDeviceLost) {
                sync::fail_present(record.state, idle);
                return;
            }
            if (record.released || sync::present_terminal(record.state) || record.state.status == sync::PresentStatus::PENDING)
                return;
            if (record.state.strict) {
                if (wait)
                    sync::wait_present(record.state);
                else
                    sync::poll_present(record.state);
            } else if (!sync::poll_present(record.state) && wait && idle == vk::Result::eSuccess &&
                       record.state.status == sync::PresentStatus::SUBMITTED)
                record.state.status = sync::PresentStatus::LEGACY_RELEASED;
        }
        bool matches_presentation_scope(const PresentRecord& record, const std::shared_ptr<impl::SurfaceImpl>& surface,
                                        std::span<const std::shared_ptr<SurfacePresentationDomain>> domains) noexcept {
            if (!record.generation)
                return false;
            if (surface)
                return record.generation->surface == surface.get();
            for (const auto& domain : domains)
                if (const auto parent = domain->surface.lock(); parent && parent.get() == record.generation->surface)
                    return true;
            return false;
        }
        void prepend_present_chain(std::shared_ptr<PresentRecord>& head, std::shared_ptr<PresentRecord>& chain) noexcept {
            if (!chain)
                return;
            auto* tail = chain.get();
            while (tail->quarantine_next)
                tail = tail->quarantine_next.get();
            tail->quarantine_next = std::move(head);
            head = std::move(chain);
        }
        bool present_native_active(const PresentRecord& record) noexcept {
            const auto* context = record.generation ? record.generation->presentation_context : nullptr;
            return record.state.observing || (context && record.image < context->images.size() && context->images[record.image].accessing);
        }
    } // namespace

    std::shared_ptr<SurfacePresentationDomain> presentation_domain(const std::shared_ptr<impl::SwapchainImpl>& generation) {
        require_swapchain_owner(generation);
        auto* surface = generation->surface;
        if (!surface || !surface->owner_context || !surface->owner_node)
            throw std::logic_error("Swapchain has no Surface lifetime context");
        auto& context = *surface->owner_context;
        if (auto domain = context.presentations.lock()) {
            if (domain->closing || context.presentation_identity != domain.get())
                throw std::logic_error("Surface presentation domain is closing");
            return domain;
        }
        if (context.presentation_identity)
            throw std::logic_error("Surface presentation domain is still retiring");
        auto domain = std::make_shared<SurfacePresentationDomain>();
        domain->surface = surface->owner_node->owner_ref().alias(surface).share();
        if (context.presentation_identity || !context.presentations.expired())
            throw std::logic_error("Surface presentation domain changed during construction");
        context.presentations = domain;
        context.presentation_identity = domain.get();
        return domain;
    }
    void register_presentation_domain(CommandDomain& commands, const std::shared_ptr<SurfacePresentationDomain>& domain) {
        if (commands.context.closing || !domain || domain->closing)
            throw std::logic_error("Presentation domain is unavailable");
        if (std::ranges::find(commands.presentation_domains, domain) == commands.presentation_domains.end())
            commands.presentation_domains.push_back(domain);
    }
    std::shared_ptr<AcquireEvidence> prepare_acquire_evidence(SurfacePresentationDomain& domain, const std::shared_ptr<impl::SwapchainImpl>& generation, vk::Fence fence) {
        if (domain.closing || !generation || !fence)
            throw std::logic_error("Acquire evidence requires a live generation and fence");
        auto evidence = std::make_shared<AcquireEvidence>();
        evidence->generation = generation;
        evidence->generation_address = generation.get();
        evidence->proof.origin = generation->device;
        evidence->proof.device = generation->native_device;
        evidence->proof.fence = fence;
        domain.acquisitions.push_back(evidence);
        return evidence;
    }
    void commit_acquire_evidence(const std::shared_ptr<AcquireEvidence>& source, impl::SwapchainImpl& generation, std::uint32_t image) {
        auto& evidence = *source;
        evidence.proof.status = sync::FenceProofStatus::PENDING;
        evidence.finished = true;
        if (!generation.presentation_context || image >= generation.presentation_context->images.size())
            throw std::runtime_error("Acquired image is outside its generation");
        auto& slot = generation.presentation_context->images[image];
        if (slot.present_serial && slot.device != generation.native_device)
            throw std::logic_error("Prior presentation device changed");
        evidence.image = image;
        evidence.present_serial = slot.present_serial;
        evidence.queue = slot.queue;
        if (auto previous = slot.record.lock(); previous && !previous->state.strict && !previous->released)
            previous->state.reacquire = std::shared_ptr<sync::FenceProof>(source, &evidence.proof);
    }
    std::shared_ptr<PresentRecord> prepare_present_record(SurfacePresentationDomain& domain, std::shared_ptr<impl::SwapchainImpl> generation,
                                                          std::shared_ptr<impl::SemaphoreImpl> semaphore, std::shared_ptr<sync::SemaphoreWSIUse> marker,
                                                          vk::Queue queue, std::uint32_t image) {
        require_swapchain_owner(generation);
        if (domain.closing || !generation->presentation_context || image >= generation->presentation_context->images.size())
            throw std::logic_error("Presentation requires a live image slot");
        auto& context = *generation->presentation_context;
        auto& slot = context.images[image];
        // 例外時も、slotの借用guardを解いてからそのownerを破棄する。
        std::shared_ptr<PresentRecord> record;
        const ImagePresentAccess access(slot);
        const auto previous = slot.record.lock();
        if (previous && !previous->released && previous->state.status == sync::PresentStatus::UNCERTAIN)
            sync::poll_present(previous->state);
        if (previous && !previous->released && !sync::present_terminal(previous->state)) {
            if (previous->state.status == sync::PresentStatus::PENDING || previous->state.status == sync::PresentStatus::UNCERTAIN ||
                (previous->state.strict && !sync::wait_present(previous->state)))
                throw std::logic_error("Previous presentation is not complete");
        }
        if (slot.epoch == std::numeric_limits<std::uint64_t>::max())
            throw std::overflow_error("Presentation image epoch overflow");
        vk::Semaphore wait;
        if (!semaphore) {
            semaphore = swapchain_finished(generation);
            marker = sync::track_semaphore_wsi(*semaphore);
        }
        require_semaphore_owner(semaphore);
        if (semaphore->device != generation->device || semaphore->semaphores.size() <= image || !marker || marker->acquire)
            throw std::invalid_argument("Present semaphore does not match its image generation");
        sync::require_semaphore_element(*semaphore, image);
        wait = semaphore->semaphores[image];
        record = std::make_shared<PresentRecord>();
        record->state.origin = generation->device;
        record->state.device = generation->native_device;
        record->state.strict = generation->present_fences == PresentFencePolicy::REQUIRE_FENCE;
        record->queue = queue;
        record->wait_semaphore = wait;
        record->image = image;
        record->epoch = ++slot.epoch;
        record->generation = std::move(generation);
        record->marker = std::move(marker);
        record->semaphore_source = semaphore.get();
        if (semaphore && (semaphore.owner_before(record->generation) || record->generation.owner_before(semaphore)))
            record->semaphore = std::move(semaphore);
        domain.records.push_back(record);
        slot.record = record;
        try {
            if (record->state.strict) {
                if (!context.fences || image >= context.fences->fences.size())
                    throw std::logic_error("Generation has no present fence pool");
                reserve_present_fence({record->generation, context.fences}, image, {record, &record->state});
            }
        } catch (...) {
            if (slot.epoch == record->epoch && slot.record.lock() == record)
                slot.record = previous;
            std::erase(domain.records, record);
            throw;
        }
        return record;
    }
    sync::PresentEnqueue enqueue_present_record(PresentRecord& record) {
        if (record.released || !record.generation || record.state.status != sync::PresentStatus::PENDING)
            throw std::logic_error("Presentation is not pending");
        auto& slot = record.generation->presentation_context->images.at(record.image);
        const ImagePresentAccess access(slot);
        if (slot.epoch != record.epoch || slot.record.lock().get() != &record)
            throw std::logic_error("Presentation image reservation changed");
        if (!record.semaphore_source)
            throw std::logic_error("Presentation semaphore has been released");
        if (record.marker)
            sync::require_present_wait(*record.semaphore_source, record.image, *record.marker);
        else
            sync::require_semaphore_element(*record.semaphore_source, record.image);
        record.serial = next_present_serial();
        if (record.semaphore_source->use.accessing)
            throw std::logic_error("Presentation semaphore source is being accessed");
        const NativeCall semaphore_call(record.semaphore_source->use.accessing);
        auto result = record.generation->device->lost ? vk::Result::eErrorDeviceLost
                                                      : operation::queue_present(record.state.device, record.queue, record.generation->swap_chain, record.image,
                                                                                 record.wait_semaphore, record.state.strict ? record.state.proof : vk::Fence{});
        if (record.generation->device->lost)
            result = vk::Result::eErrorDeviceLost;
        const auto outcome = sync::commit_present(record.state, result);
        if (outcome == sync::PresentEnqueue::ENQUEUED) {
            slot.present_serial = record.serial;
            slot.device = record.state.device;
            slot.queue = record.queue;
        }
        return outcome;
    }
    void collect_presentations(std::shared_ptr<SurfacePresentationDomain> domain) noexcept {
        if (!domain || domain->closing || domain->recovering)
            return;
        try {
            PresentationSnapshot snapshot{domain, domain->records, domain->acquisitions};
            apply_acquisitions(snapshot.records, snapshot.acquisitions);
            for (const auto& record : snapshot.records)
                if (!record->released)
                    sync::poll_present(record->state);
            retire_presentations(snapshot);
        } catch (...) {
            // snapshot／回収一覧を確保できない間も、資源の保持を維持する。
        }
    }
    size_t quarantined_presentation_count() noexcept {
        auto& registry = presentation_quarantine();
        const std::lock_guard lock(registry.mutex);
        return registry.count;
    }
    QuarantinedPresentations::QuarantinedPresentations(std::shared_ptr<impl::LogicalDeviceImpl> device,
                                                       std::span<const std::shared_ptr<SurfacePresentationDomain>> domains,
                                                       std::shared_ptr<impl::SurfaceImpl> surface)
        : _device(std::move(device)), _surface(std::move(surface)), _domains(domains) {
        auto& registry = presentation_quarantine();
        const std::lock_guard lock(registry.mutex);
        if (registry.collecting)
            throw std::logic_error("Presentation quarantine is already being observed");
        registry.collecting = true;
        _collecting = true;
        auto pending = std::move(registry.head);
        while (pending) {
            auto record = std::move(pending);
            pending = std::move(record->quarantine_next);
            auto& head = record->state.device == _device->device && matches_presentation_scope(*record, _surface, _domains) ? _selected : _foreign;
            record->quarantine_next = std::move(head);
            head = std::move(record);
        }
    }
    QuarantinedPresentations::~QuarantinedPresentations() {
        if (_collecting) {
            auto& registry = presentation_quarantine();
            const std::lock_guard lock(registry.mutex);
            prepend_present_chain(registry.head, _selected);
            prepend_present_chain(registry.head, _foreign);
            registry.collecting = false;
        }
        release();
    }
    void QuarantinedPresentations::save_observations(vk::Result idle, bool wait) noexcept {
        if (_device->lost)
            idle = vk::Result::eErrorDeviceLost;
        if (idle == vk::Result::eErrorDeviceLost) {
            for (auto* head : {_selected.get(), _foreign.get()})
                for (auto* record = head; record; record = record->quarantine_next.get())
                    if (record->state.device == _device->device)
                        sync::fail_present(record->state, idle);
            return;
        }
        for (auto* record = _selected.get(); record; record = record->quarantine_next.get()) {
            observe_present_record(*record, idle, wait);
            if (_device->lost) {
                save_observations(vk::Result::eErrorDeviceLost, false);
                return;
            }
        }
    }
    void QuarantinedPresentations::bookkeep() noexcept {
        if (!_collecting)
            return;
        std::shared_ptr<PresentRecord> pending;
        while (_selected) {
            auto record = std::move(_selected);
            _selected = std::move(record->quarantine_next);
            auto& head = sync::present_terminal(record->state) && !present_native_active(*record) ? _ready : pending;
            record->quarantine_next = std::move(head);
            head = std::move(record);
        }
        _pending = bool(pending);
        auto& registry = presentation_quarantine();
        const std::lock_guard lock(registry.mutex);
        prepend_present_chain(registry.head, pending);
        prepend_present_chain(registry.head, _foreign);
        for (auto* record = _ready.get(); record; record = record->quarantine_next.get()) {
            record->quarantined = false;
            record->released = true;
            --registry.count;
        }
        registry.collecting = false;
        _collecting = false;
    }
    void QuarantinedPresentations::release() noexcept {
        while (_ready) {
            auto record = std::move(_ready);
            _ready = std::move(record->quarantine_next);
            const auto owners = detach_present_owners(*record);
        }
    }
    bool QuarantinedPresentations::pending() const noexcept {
        if (!_collecting)
            return _pending;
        for (auto* record = _selected.get(); record; record = record->quarantine_next.get())
            if (!sync::present_terminal(record->state) || present_native_active(*record))
                return true;
        return false;
    }
    void collect_quarantined_presentations(const std::weak_ptr<impl::SurfaceImpl>& surface, vk::Device device, vk::Result known_idle) noexcept {
        const auto parent = surface.lock();
        if (!parent)
            return;
        auto& registry = presentation_quarantine();
        std::shared_ptr<PresentRecord> pending, retained;
        {
            const std::lock_guard lock(registry.mutex);
            if (registry.collecting)
                return;
            registry.collecting = true;
            pending = std::move(registry.head);
        }
        while (pending) {
            auto record = std::move(pending);
            pending = std::move(record->quarantine_next);
            if (!record->generation || record->state.device != device || record->generation->surface != parent.get() ||
                !parent->owner_node || !parent->owner_node->matches(parent)) {
                record->quarantine_next = std::move(retained);
                retained = std::move(record);
                continue;
            }
            if (known_idle == vk::Result::eErrorDeviceLost) {
                record->state.result = vk::Result::eErrorDeviceLost;
                record->state.status = sync::PresentStatus::FAILED;
            } else
                sync::poll_present(record->state);
            if (sync::present_terminal(record->state) && !present_native_active(*record)) {
                record->quarantined = false;
                const auto owners = detach_present_owners(*record);
                const std::lock_guard lock(registry.mutex);
                --registry.count;
            } else {
                record->quarantine_next = std::move(retained);
                retained = std::move(record);
            }
        }
        const std::lock_guard lock(registry.mutex);
        if (retained) {
            auto* tail = retained.get();
            while (tail->quarantine_next)
                tail = tail->quarantine_next.get();
            tail->quarantine_next = std::move(registry.head);
            registry.head = std::move(retained);
        }
        registry.collecting = false;
    }
    PresentationSnapshot snapshot_presentations(const std::shared_ptr<SurfacePresentationDomain>& domain, vk::Device device) {
        PresentationSnapshot snapshot;
        snapshot.domain = domain;
        for (const auto& evidence : domain->acquisitions)
            if (evidence->proof.device == device)
                snapshot.acquisitions.push_back(evidence);
        for (const auto& record : domain->records)
            if (!record->released && record->state.status != sync::PresentStatus::PENDING && record->state.device == device)
                snapshot.records.push_back(record);
        return snapshot;
    }
    void finish_presentations(PresentationSnapshot& snapshot, vk::Result idle_result) {
        if (snapshot.domain->recovering)
            return;
        if (idle_result == vk::Result::eErrorDeviceLost)
            for (const auto& record : snapshot.records)
                sync::fail_present(record->state, idle_result);
        else
            apply_acquisitions(snapshot.records, snapshot.acquisitions);
        for (const auto& record : snapshot.records) {
            if (record->released || sync::present_terminal(record->state))
                continue;
            if (idle_result == vk::Result::eErrorDeviceLost) {
                record->state.result = idle_result;
                record->state.status = sync::PresentStatus::FAILED;
                continue;
            }
            if (record->state.strict) {
                if (!sync::wait_present(record->state) && !sync::present_terminal(record->state))
                    throw std::runtime_error("Present fence completion could not be established");
            } else if (!sync::poll_present(record->state)) {
                if (record->state.status == sync::PresentStatus::UNCERTAIN)
                    throw std::runtime_error("Presentation enqueue outcome is unknown");
                if (idle_result == vk::Result::eSuccess)
                    record->state.status = sync::PresentStatus::LEGACY_RELEASED;
            }
        }
        retire_presentations(snapshot);
    }
    void observe_presentations(PresentationSnapshot& snapshot, vk::Result idle_result, bool wait) noexcept {
        if (idle_result == vk::Result::eErrorDeviceLost) {
            for (const auto& evidence : snapshot.acquisitions)
                sync::save_fence_observation(evidence->proof, idle_result);
            for (const auto& record : snapshot.records)
                sync::fail_present(record->state, idle_result);
            return;
        }
        apply_acquisitions(snapshot.records, snapshot.acquisitions);
        for (const auto& record : snapshot.records) {
            if (record->released || sync::present_terminal(record->state) || record->state.status == sync::PresentStatus::PENDING)
                continue;
            if (record->state.strict) {
                if (wait)
                    sync::wait_present(record->state);
                else
                    sync::poll_present(record->state);
            } else if (!sync::poll_present(record->state) && wait && idle_result == vk::Result::eSuccess &&
                       record->state.status == sync::PresentStatus::SUBMITTED)
                record->state.status = sync::PresentStatus::LEGACY_RELEASED;
        }
    }
    void bookkeep_presentations(PresentationSnapshot& snapshot) noexcept {
        for (const auto& record : snapshot.records)
            if (sync::present_terminal(record->state) && !present_native_active(*record)) {
                record->released = true;
                if (record->marker && record->state.status != sync::PresentStatus::CANCELLED)
                    record->marker->complete = true;
            }
        auto& domain = *snapshot.domain;
        std::erase_if(domain.records, [&](const auto& record) {
            return record->released && std::ranges::find(snapshot.records, record) != snapshot.records.end();
        });
        std::erase_if(domain.acquisitions, [&](const auto& evidence) {
            return evidence->finished && sync::fence_proof_terminal(evidence->proof) &&
                   std::ranges::find(snapshot.acquisitions, evidence) != snapshot.acquisitions.end();
        });
    }
    void release_presentations(PresentationSnapshot& snapshot) noexcept {
        for (const auto& record : snapshot.records)
            if (record->released && record->generation) {
                const auto owners = detach_present_owners(*record);
            }
    }
    SurfacePresentationDomain::~SurfacePresentationDomain() {
        closing = true;
        const auto tickets = quarantine_acquisitions(*this);
        const auto parent = surface.lock();
        auto* context = parent ? parent->owner_context : nullptr;
        auto pending = std::move(records);
        auto evidence = std::move(acquisitions);
        apply_acquisitions(pending, evidence);
        for (const auto& record : pending) {
            if (record->released || sync::present_terminal(record->state))
                continue;
            if (record->state.origin && record->state.origin->lost) {
                sync::fail_present(record->state, vk::Result::eErrorDeviceLost);
                continue;
            }
            if (record->state.status == sync::PresentStatus::PENDING) {
                if (!record->state.observing)
                    record->state.status = sync::PresentStatus::CANCELLED;
                else
                    quarantine_presentation(record);
                continue;
            }
            if (sync::NativeDeviceCall::Active(record->state.device)) {
                quarantine_presentation(record);
                continue;
            }
            if (record->state.status == sync::PresentStatus::UNCERTAIN) {
                sync::poll_present(record->state);
                if (!sync::present_terminal(record->state)) {
                    if (operation::wait_device_idle(record->state.device) == vk::Result::eErrorDeviceLost) {
                        record->state.result = vk::Result::eErrorDeviceLost;
                        record->state.status = sync::PresentStatus::FAILED;
                        if (record->state.origin)
                            record->state.origin->lost = true;
                    } else
                        quarantine_presentation(record);
                }
                continue;
            }
            if (!record->state.strict) {
                const auto result = operation::wait_device_idle(record->state.device);
                if (result == vk::Result::eErrorDeviceLost) {
                    sync::fail_present(record->state, result);
                    if (record->state.origin)
                        record->state.origin->lost = true;
                } else if (result == vk::Result::eSuccess)
                    record->state.status = sync::PresentStatus::LEGACY_RELEASED;
                else
                    quarantine_presentation(record);
            } else {
                if (!sync::present_terminal(record->state) && record->state.status == sync::PresentStatus::SUBMITTED)
                    sync::wait_present(record->state);
                if (!sync::present_terminal(record->state))
                    quarantine_presentation(record);
            }
        }
        for (const auto& ticket : tickets)
            if (ticket->device && ticket->device->lost)
                recover_acquisition(*ticket);
        for (const auto& record : pending) {
            if (record->state.origin && record->state.origin->lost)
                sync::fail_present(record->state, vk::Result::eErrorDeviceLost);
            if (!record->quarantined) {
                record->released = true;
                if (record->marker && record->state.status != sync::PresentStatus::CANCELLED)
                    record->marker->complete = true;
            }
        }
        for (const auto& item : evidence)
            if (item->proof.origin && item->proof.origin->lost)
                sync::save_fence_observation(item->proof, vk::Result::eErrorDeviceLost);
        for (const auto& ticket : tickets)
            if (!ticket->quarantined)
                release_acquire_owners(*ticket);
        for (const auto& record : pending)
            if (record->released && !record->quarantined && record->generation) {
                const auto released = detach_present_owners(*record);
            }
        if (context && context->presentation_identity == this) {
            context->presentations.reset();
            context->presentation_identity = nullptr;
        }
    }
} // namespace VSL_NAMESPACE::lifetime
