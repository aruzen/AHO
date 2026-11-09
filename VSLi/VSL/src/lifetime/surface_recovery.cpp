#include <VSL/lifetime/surface_recovery.hpp>
#include <VSL/lifetime/recovery_batch.hpp>
#include <VSL/lifetime/acquire_ticket.hpp>
#include <VSL/lifetime/command_domain.hpp>
#include <VSL/lifetime/device_owner.hpp>
#include <VSL/lifetime/surface_storage.hpp>
#include <VSL/operation/submission_ops.hpp>
#include <VSL/sync/submission_completion.hpp>
#include <VSL/sync/device_call.hpp>
#include <VSL/vulkan/surface.hpp>
#include <algorithm>
#include <stdexcept>
#include <utility>

namespace VSL_NAMESPACE::lifetime {
    namespace {
        struct RecoveryResult {
            SurfaceRecoveryStatus status;
            bool presentation_pending = false;
            bool pending = false;
            bool captured = false;
        };
        struct RetirementNode {
            std::weak_ptr<impl::LogicalDeviceImpl> device;
            std::weak_ptr<impl::SurfaceImpl> surface;
            impl::LogicalDeviceImpl* device_slot = nullptr;
            impl::SurfaceImpl* surface_slot = nullptr;
            std::unique_ptr<RetirementNode> next;
        };
        struct RetirementQueue {
            std::unique_ptr<RetirementNode> head;
            bool collecting = false;
            ~RetirementQueue();
        };
        RetirementQueue& retirement_queue();
        bool same_retirement(const RetirementNode& a, const RetirementNode& b) noexcept;
        void push_retirement(std::unique_ptr<RetirementNode>& head, std::unique_ptr<RetirementNode> node) noexcept;
        void restore_retirements(RetirementQueue& queue, std::unique_ptr<RetirementNode> source) noexcept;
        struct RetirementBatch {
            RetirementQueue& queue;
            std::unique_ptr<RetirementNode> selected, pending;
            explicit RetirementBatch(RetirementQueue& queue);
            ~RetirementBatch();
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
        RecoveryResult recover_surface_scope(LogicalDeviceAccessor device, const Surface& surface, SurfaceRecoveryMode mode, bool observe_graphics);
        RetirementQueue::~RetirementQueue() {
            while (head) {
                auto node = std::move(head);
                head = std::move(node->next);
            }
        }
        RetirementQueue& retirement_queue() {
            static thread_local RetirementQueue QUEUE;
            return QUEUE;
        }
        bool same_retirement(const RetirementNode& a, const RetirementNode& b) noexcept {
            return a.device_slot == b.device_slot && a.surface_slot == b.surface_slot &&
                   !a.device.owner_before(b.device) && !b.device.owner_before(a.device) &&
                   !a.surface.owner_before(b.surface) && !b.surface.owner_before(a.surface);
        }
        void push_retirement(std::unique_ptr<RetirementNode>& head, std::unique_ptr<RetirementNode> node) noexcept {
            node->next = std::move(head);
            head = std::move(node);
        }
        void restore_retirements(RetirementQueue& queue, std::unique_ptr<RetirementNode> source) noexcept {
            while (source) {
                auto node = std::move(source);
                source = std::move(node->next);
                bool duplicate = false;
                for (auto* current = queue.head.get(); current; current = current->next.get())
                    duplicate |= same_retirement(*node, *current);
                if (!duplicate)
                    push_retirement(queue.head, std::move(node));
            }
        }
        RetirementBatch::RetirementBatch(RetirementQueue& queue) : queue(queue), selected(std::move(queue.head)) {
            queue.collecting = true;
        }
        RetirementBatch::~RetirementBatch() {
            restore_retirements(queue, std::move(selected));
            restore_retirements(queue, std::move(pending));
            queue.collecting = false;
        }
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
            if (sync::NativeDeviceCall::Active(device->device))
                return {device->lost ? SurfaceRecoveryStatus::DEVICE_LOST : SurfaceRecoveryStatus::PENDING, true, true};
            if (std::ranges::any_of(domains, [](const auto& domain) { return domain->recovering; }))
                return {SurfaceRecoveryStatus::PENDING, true, true};
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
                return {SurfaceRecoveryStatus::PENDING, true, true};
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
            const bool captured = acquisitions.captured() || work->captured() || orphan_present.captured() || !known.empty() ||
                                  std::ranges::any_of(presentation, [](const auto& snapshot) { return !snapshot.records.empty() || !snapshot.acquisitions.empty(); });
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
                    presentation_pending, pending, captured};
        }
        RecoveryResult recover_surface_scope(LogicalDeviceAccessor device, const Surface& surface, SurfaceRecoveryMode mode, bool observe_graphics) {
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
            return recover_batch(native_device, parent, {}, domains, {}, mode == SurfaceRecoveryMode::WAIT, observe_graphics);
        }
    } // namespace
    SurfaceRecoveryStatus recover_surface(LogicalDeviceAccessor device, const Surface& surface, SurfaceRecoveryMode mode) {
        return recover_surface_scope(device, surface, mode, true).status;
    }
    void queue_surface_retirement(LogicalDeviceAccessor device, const Surface& surface) {
        require_device_owner(device);
        (void)surface.owner_ref();
        if (!surface.data_ || !surface.data_->surface || !surface.data_->owner_context || device.data_->parent_device->vulkan != surface.data_->vulkan)
            throw std::invalid_argument("Surface retirement requires its Device Instance");
        auto& queue = retirement_queue();
        RetirementNode key{device.data_, surface.data_, device.data_.get(), surface.data_.get(), {}};
        for (auto* node = queue.head.get(); node; node = node->next.get())
            if (same_retirement(*node, key))
                return;
        push_retirement(queue.head, std::make_unique<RetirementNode>(std::move(key)));
    }
    SurfaceRecoveryStatus collect_surface_retirements(LogicalDeviceAccessor device, SurfaceRecoveryMode mode) {
        require_device_owner(device);
        if (mode != SurfaceRecoveryMode::POLL && mode != SurfaceRecoveryMode::WAIT)
            throw std::invalid_argument("Unknown Surface recovery mode");
        auto& queue = retirement_queue();
        if (queue.collecting || sync::NativeDeviceCall::Active(device.data_->device))
            return device.data_->lost ? SurfaceRecoveryStatus::DEVICE_LOST : SurfaceRecoveryStatus::PENDING;
        RetirementBatch batch(queue);
        auto status = device.data_->lost ? SurfaceRecoveryStatus::DEVICE_LOST : SurfaceRecoveryStatus::COMPLETE;
        while (batch.selected) {
            auto node = std::move(batch.selected);
            batch.selected = std::move(node->next);
            const auto parent_device = node->device.lock();
            const auto parent_surface = node->surface.lock();
            if (!parent_device || !parent_surface)
                continue;
            bool keep = true;
            if (parent_device == device.data_ && !parent_device.owner_before(device.data_) && !device.data_.owner_before(parent_device)) {
                try {
                    auto result = recover_surface_scope(device, Surface(parent_surface), mode, true);
                    if (!result.pending && result.captured)
                        result = recover_surface_scope(device, Surface(parent_surface), SurfaceRecoveryMode::POLL, true);
                    keep = result.pending || result.captured;
                    if (result.status == SurfaceRecoveryStatus::DEVICE_LOST)
                        status = SurfaceRecoveryStatus::DEVICE_LOST;
                    else if (keep && status == SurfaceRecoveryStatus::COMPLETE)
                        status = SurfaceRecoveryStatus::PENDING;
                } catch (...) {
                    if (status == SurfaceRecoveryStatus::COMPLETE)
                        status = SurfaceRecoveryStatus::PENDING;
                }
            }
            if (keep)
                push_retirement(batch.pending, std::move(node));
        }
        if (status == SurfaceRecoveryStatus::COMPLETE)
            for (auto* node = queue.head.get(); node; node = node->next.get())
                if (node->device_slot == device.data_.get() && !node->device.owner_before(device.data_) && !device.data_.owner_before(node->device))
                    status = SurfaceRecoveryStatus::PENDING;
        return status;
    }
    SurfaceRetirementCollection::SurfaceRetirementCollection(std::shared_ptr<impl::LogicalDeviceImpl> device) noexcept
        : _device(std::move(device)) {}
    SurfaceRetirementCollection::~SurfaceRetirementCollection() {
        if (!_device || !retirement_queue().head)
            return;
        try {
            (void)collect_surface_retirements(LogicalDeviceAccessor{_device});
        } catch (...) {
        }
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
        if (sync::NativeDeviceCall::Active(ticket.device->device))
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
