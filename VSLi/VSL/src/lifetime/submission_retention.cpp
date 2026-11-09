#include <VSL/lifetime/command_domain.hpp>
#include <VSL/lifetime/submission_retention.hpp>
#include <VSL/vulkan/_pimpls.hpp>
#include <VSL/sync/submission_completion.hpp>
#include <VSL/sync/resource_use_state.hpp>
#include <VSL/sync/resource_use_access.hpp>
#include <VSL/lifetime/descriptor_bindings.hpp>
#include <VSL/lifetime/owner_access.hpp>
#include <VSL/lifetime/device_owner.hpp>
#include <VSL/lifetime/command_storage.hpp>
#include <VSL/lifetime/presentation_retention.hpp>
#include <VSL/lifetime/surface_storage.hpp>
#include <VSL/operation/submission_ops.hpp>
#include <algorithm>
#include <stdexcept>
#include <mutex>
#include <new>

namespace VSL_NAMESPACE::lifetime {
    namespace {
        struct SubmissionQuarantine {
            std::mutex mutex;
            std::shared_ptr<impl::SubmissionWork> head;
            size_t count = 0;
            bool collecting = false;
        };
        SubmissionQuarantine& submission_quarantine() noexcept;
        void prepend_submission(std::shared_ptr<impl::SubmissionWork>& head, std::shared_ptr<impl::SubmissionWork> work) noexcept;
        void prepend_chain(std::shared_ptr<impl::SubmissionWork>& head, std::shared_ptr<impl::SubmissionWork>& chain) noexcept;
        void unregister_submission(impl::SubmissionWork& work, SubmissionQuarantine& registry) noexcept;
        bool matches_scope(const impl::SubmissionWork& work, const std::shared_ptr<impl::CommandBufferImpl>& ring,
                           std::span<const std::shared_ptr<SurfacePresentationDomain>> presentations) noexcept;
        struct DescriptorRecording {
            impl::GraphicResourceImpl& resource;
            explicit DescriptorRecording(impl::GraphicResourceImpl& resource);
            ~DescriptorRecording() { resource.recording = false; }
        };
        bool contains_owner(const std::vector<std::shared_ptr<void>>& resources, const std::shared_ptr<void>& resource);
        DescriptorRecording::DescriptorRecording(impl::GraphicResourceImpl& resource) : resource(resource) {
            require_descriptor_valid(resource);
            if (resource.recording)
                throw std::logic_error("Descriptor resource is being recorded");
            if (resource.owner_context && resource.owner_context->node.updating())
                throw std::logic_error("Ownership root is being updated");
            resource.recording = true;
        }
        bool contains_owner(const std::vector<std::shared_ptr<void>>& resources, const std::shared_ptr<void>& resource) {
            return std::ranges::any_of(resources, [&](const auto& existing) {
                return !existing.owner_before(resource) && !resource.owner_before(existing);
            });
        }
        SubmissionQuarantine& submission_quarantine() noexcept {
            static_assert(std::is_nothrow_default_constructible_v<SubmissionQuarantine>);
            alignas(SubmissionQuarantine) static std::byte STORAGE[sizeof(SubmissionQuarantine)];
            static auto* const REGISTRY = std::construct_at(reinterpret_cast<SubmissionQuarantine*>(STORAGE));
            return *REGISTRY;
        }
        void prepend_submission(std::shared_ptr<impl::SubmissionWork>& head, std::shared_ptr<impl::SubmissionWork> work) noexcept {
            work->quarantine_next = std::move(head);
            head = std::move(work);
        }
        void prepend_chain(std::shared_ptr<impl::SubmissionWork>& head, std::shared_ptr<impl::SubmissionWork>& chain) noexcept {
            if (!chain)
                return;
            auto* tail = chain.get();
            while (tail->quarantine_next)
                tail = tail->quarantine_next.get();
            tail->quarantine_next = std::move(head);
            head = std::move(chain);
        }
        void unregister_submission(impl::SubmissionWork& work, SubmissionQuarantine& registry) noexcept {
            work.quarantined = false;
            --registry.count;
            if (work.native_buffer && work.native_buffer->owner_context)
                --work.native_buffer->owner_context->unresolved_submissions;
        }
        bool matches_scope(const impl::SubmissionWork& work, const std::shared_ptr<impl::CommandBufferImpl>& ring,
                           std::span<const std::shared_ptr<SurfacePresentationDomain>> presentations) noexcept {
            if (work.native_buffer == ring.get() && ring->owner_node && ring->owner_node->matches(ring))
                return true;
            if (!work.surface_address)
                return true; // headlessはDevice単位。Windowのowner解放だけSurface scopeへ限定する。
            for (const auto& presentation : presentations) {
                const auto surface = presentation->surface.lock();
                if (surface.get() == work.surface_address && !surface.owner_before(work.surface) && !work.surface.owner_before(surface))
                    return true;
            }
            return false;
        }
    } // namespace
    std::shared_ptr<void> pin_submission_owner(const impl::SubmissionWork& work) noexcept {
        auto* const buffer = work.native_buffer;
        if (!buffer)
            return {};
        if (buffer->owner_context)
            if (const auto domain = buffer->owner_context->domain.lock(); domain && domain->buffer.get() == buffer)
                return domain;
        try {
            if (buffer->owner_node)
                return buffer->owner_node->owner_ref().share();
        } catch (...) {
        }
        return {};
    }
    SubmissionObservation::SubmissionObservation(impl::SubmissionWork& work) noexcept {
        _owner = pin_submission_owner(work);
        if (work.result->native.active || work.result->native.observing)
            return;
        if (work.result->status != Completion::Status::SUBMITTED && !sync::native_pending(*work.result))
            return;
        _result = work.result.get();
        _result->native.observing = true;
    }
    SubmissionObservation::~SubmissionObservation() {
        finish();
    }
    SubmissionObservation::SubmissionObservation(SubmissionObservation&& other) noexcept
        : _owner(std::move(other._owner)), _result(std::exchange(other._result, nullptr)) {}
    void SubmissionObservation::finish() noexcept {
        if (auto* result = std::exchange(_result, nullptr))
            result->native.observing = false;
    }
    SubmissionTransaction::SubmissionTransaction(CommandDomain& domain, impl::SubmissionWork& work)
        : _domain(domain), _work(work), _owns_context(domain.context.work == &work),
          _native_call(work.device ? static_cast<VkDevice>(work.device->device) : VK_NULL_HANDLE) {
        if (work.result->native.active || (_owns_context && domain.context.submitting))
            throw std::logic_error("Submission transaction is already active");
        work.result->native.active = true;
        if (_owns_context) {
            domain.context.submitting = true;
            domain.context.recording = false;
        }
    }
    SubmissionTransaction::~SubmissionTransaction() {
        finish();
    }
    void SubmissionTransaction::finish() noexcept {
        if (!_active)
            return;
        _work.result->native.active = false;
        if (_owns_context)
            _domain.context.submitting = false;
        _active = false;
        _native_call.finish();
    }
    void detach_recording_cpu(CommandDomain& domain, impl::SubmissionWork& work) noexcept {
        if (domain.context.work != &work)
            return;
        domain.context.recording = false;
        domain.context.work = nullptr;
        domain.recording_submission.reset();
    }
    void abort_recording(CommandDomain& domain, impl::SubmissionWork& work, std::uint32_t slot) noexcept {
        if (domain.context.work != &work)
            return;
        if (!sync::native_pending(*work.result) && !domain.buffer->command_pool->device->lost) {
            try {
                operation::reset_command_buffer(domain.buffer->command_buffers.at(slot));
            } catch (...) {
            }
        }
        detach_recording_cpu(domain, work);
    }
    void fail_recording(CommandDomain& domain, impl::SubmissionWork& work, std::uint32_t slot,
                        Completion::Failure failure, std::int32_t error_code) noexcept {
        {
            SubmissionTransaction transaction(domain, work);
            if (work.result->native.enqueue == sync::SubmissionEnqueue::NOT_CALLED) {
                work.result->native.enqueue = sync::SubmissionEnqueue::NOT_ENQUEUED;
                work.result->native.complete = true;
                work.result->native.observation = error_code;
            }
            if (failure == Completion::Failure::DEVICE_LOST)
                sync::save_native_idle(work, static_cast<std::int32_t>(vk::Result::eErrorDeviceLost));
            abort_recording(domain, work, slot);
            sync::save_terminal(work, Completion::Status::FAILED, failure, error_code);
        }
        sync::finish_submission(work);
    }
    void quarantine_submission(const std::shared_ptr<impl::SubmissionWork>& work, const std::shared_ptr<impl::CommandBufferImpl>& buffer) noexcept {
        if (work->quarantined)
            return;
        const std::shared_ptr<void> owner = buffer;
        if (!contains_owner(work->resources, owner) && !contains_owner(work->completion_resources, owner))
            work->unresolved_buffer_owner = owner;
        work->native_buffer = buffer.get();
        work->release_resources = release_completed_resources;
        auto& registry = submission_quarantine();
        const std::lock_guard lock(registry.mutex);
        work->quarantined = true;
        ++buffer->owner_context->unresolved_submissions;
        ++registry.count;
        prepend_submission(registry.head, work);
    }
    std::shared_ptr<impl::SubmissionWork> unquarantine_submission(impl::SubmissionWork& work) noexcept {
        if (work.result->native.active || sync::native_pending(*work.result))
            return {};
        std::shared_ptr<impl::SubmissionWork> removed;
        auto& registry = submission_quarantine();
        {
            const std::lock_guard lock(registry.mutex);
            auto* link = &registry.head;
            while (*link && link->get() != &work)
                link = &(*link)->quarantine_next;
            if (!*link)
                return {};
            removed = std::move(*link);
            *link = std::move(removed->quarantine_next);
            unregister_submission(work, registry);
        }
        return removed;
    }
    size_t quarantined_submission_count() noexcept {
        auto& registry = submission_quarantine();
        const std::lock_guard lock(registry.mutex);
        return registry.count;
    }
    void inherit_quarantined_submissions(CommandDomain& domain) {
        if (!domain.buffer->owner_context->unresolved_submissions)
            return;
        auto& registry = submission_quarantine();
        {
            const std::lock_guard lock(registry.mutex);
            if (registry.collecting)
                throw std::logic_error("Command ring uncertainty is being observed");
            for (auto* link = &registry.head; *link; link = &(*link)->quarantine_next) {
                const auto& work = *link;
                if (work->native_buffer == domain.buffer.get() && domain.buffer->owner_node->matches(domain.buffer)) {
                    if (work->frame_slot >= domain.submissions.size() || domain.submissions[work->frame_slot])
                        throw std::logic_error("Unresolved submission does not match its original ring slot");
                    domain.submissions[work->frame_slot] = work;
                }
            }
        }
        for (const auto& work : domain.submissions)
            if (work && work->surface_address) {
                const auto surface = work->surface.lock();
                if (surface && surface->owner_context)
                    if (const auto presentation = surface->owner_context->presentations.lock(); presentation && !presentation->closing)
                        register_presentation_domain(domain, presentation);
            }
    }
    SubmissionSnapshot::SubmissionSnapshot(const std::shared_ptr<impl::CommandBufferImpl>& ring,
                                           std::span<const std::shared_ptr<SurfacePresentationDomain>> presentations)
        : _device(device_from_impl(*ring->command_pool->device).data_) {
        capture(ring, presentations);
    }
    SubmissionSnapshot::SubmissionSnapshot(std::shared_ptr<impl::LogicalDeviceImpl> device, std::shared_ptr<impl::SurfaceImpl> surface)
        : _device(std::move(device)), _surface(std::move(surface)) {
        require_device_owner(LogicalDeviceAccessor{_device});
        if (!_surface || !_surface->owner_node || !_surface->owner_node->matches(_surface) ||
            _device->parent_device->vulkan != _surface->vulkan)
            throw std::invalid_argument("Surface recovery requires its canonical Surface and Device");
        capture({}, {});
    }
    void SubmissionSnapshot::capture(const std::shared_ptr<impl::CommandBufferImpl>& ring,
                                     std::span<const std::shared_ptr<SurfacePresentationDomain>> presentations) {
        auto& registry = submission_quarantine();
        const std::lock_guard lock(registry.mutex);
        if (registry.collecting)
            throw std::logic_error("Submission quarantine is already being observed");
        const auto selected = [&](const impl::SubmissionWork& work) {
            const bool same_surface = _surface && work.surface_address == _surface.get() &&
                                      !_surface.owner_before(work.surface) && !work.surface.owner_before(_surface);
            return work.device == _device && (same_surface || (ring && matches_scope(work, ring, presentations)));
        };
        _observations.reserve(registry.count);
        for (auto* work = registry.head.get(); work; work = work->quarantine_next.get()) {
            if (selected(*work))
                _observations.emplace_back(*work);
        }
        registry.collecting = true;
        _collecting = true;
        auto pending = std::move(registry.head);
        while (pending) {
            auto work = std::move(pending);
            pending = std::move(work->quarantine_next);
            if (selected(*work))
                prepend_submission(_selected, std::move(work));
            else
                prepend_submission(_foreign, std::move(work));
        }
    }
    SubmissionSnapshot::~SubmissionSnapshot() {
        if (!_collecting) {
            release();
            return;
        }
        auto& registry = submission_quarantine();
        {
            const std::lock_guard lock(registry.mutex);
            prepend_chain(registry.head, _selected);
            prepend_chain(registry.head, _foreign);
            registry.collecting = false;
            _collecting = false;
        }
        for (auto& observation : _observations)
            observation.finish();
        release();
    }
    void SubmissionSnapshot::save_observations(std::int32_t idle_result) noexcept {
        const auto idle = _device->lost ? vk::Result::eErrorDeviceLost : static_cast<vk::Result>(idle_result);
        if (idle == vk::Result::eErrorDeviceLost) {
            _device->lost = true;
            for (const auto& head : {_selected, _foreign})
                for (auto* work = head.get(); work; work = work->quarantine_next.get())
                    if (work->device == _device && !work->result->native.active)
                        sync::save_native_idle(*work, static_cast<std::int32_t>(idle));
            return;
        }
        for (auto* work = _selected.get(); work; work = work->quarantine_next.get()) {
            if (work->result->native.active || !sync::native_pending(*work->result))
                continue;
            if (idle != vk::Result::eSuccess &&
                !std::ranges::any_of(_observations, [&](const auto& observation) { return observation.owns(*work->result); }))
                continue;
            const auto result = idle == vk::Result::eSuccess ? idle : operation::poll_fence(work->device->device, work->fence);
            if (result == vk::Result::eErrorDeviceLost) {
                save_observations(static_cast<std::int32_t>(result));
                return;
            }
            if (result == vk::Result::eSuccess || result == vk::Result::eErrorDeviceLost)
                sync::save_native_idle(*work, static_cast<std::int32_t>(result));
        }
    }
    void SubmissionSnapshot::bookkeep() noexcept {
        if (!_collecting)
            return;
        for (auto& observation : _observations)
            observation.finish();
        std::shared_ptr<impl::SubmissionWork> pending;
        while (_selected) {
            auto work = std::move(_selected);
            _selected = std::move(work->quarantine_next);
            if (!work->result->native.active && !sync::native_pending(*work->result) && sync::terminal_status(work->result->status))
                prepend_submission(_ready, std::move(work));
            else
                prepend_submission(pending, std::move(work));
        }
        auto& registry = submission_quarantine();
        {
            const std::lock_guard lock(registry.mutex);
            _pending = bool(pending);
            prepend_chain(registry.head, pending);
            prepend_chain(registry.head, _foreign);
            for (auto* work = _ready.get(); work; work = work->quarantine_next.get())
                unregister_submission(*work, registry);
            registry.collecting = false;
            _collecting = false;
        }
    }
    void SubmissionSnapshot::release() noexcept {
        while (_ready) {
            auto work = std::move(_ready);
            _ready = std::move(work->quarantine_next);
            sync::finish_submission(*work);
        }
        _observations.clear();
    }
    void SubmissionSnapshot::finish() noexcept {
        bookkeep();
        release();
    }
    void retain_resource(std::vector<std::shared_ptr<void>>& resources, const std::shared_ptr<void>& resource) {
        if (resource.use_count() && !contains_owner(resources, resource))
            resources.push_back(resource);
    }
    void retain_resource(std::vector<std::shared_ptr<void>>& resources, std::shared_ptr<void>&& resource) {
        auto accepted = std::move(resource);
        if (accepted.use_count() && !contains_owner(resources, accepted))
            resources.push_back(std::move(accepted));
    }
    void retain_until_complete(impl::SubmissionWork& submission, std::shared_ptr<void> resource) {
        if (!submission.result || !sync::recordable(*submission.result))
            throw std::logic_error("Resource retention requires a pending submission");
        if (resource.use_count()) {
            retain_resource(submission.completion_resources, std::move(resource));
            submission.release_resources = release_completed_resources;
        }
    }
    void require_recording(const CommandBuffer& buffer, const Completion& completion) {
        if (!buffer.data_ || !buffer.data_->recording_context || !buffer.data_->recording_context->work || !completion.data_ ||
            !buffer.data_->recording_context->recording || buffer.data_->recording_context->submitting ||
            buffer.data_->recording_context->work->result != completion.data_ || !sync::recordable(*completion.data_))
            throw std::logic_error("Recording phase changed while tracking resources");
    }
    void record_resource_state(const ResourceUseState& state, const CommandBuffer& buffer, const Completion& completion) {
        state.mark_used(completion);
        require_recording(buffer, completion);
        if (sync::ResourceUseAccess::RequiresKnown(state)) {
            auto& checks = buffer.data_->recording_context->work->validity_checks;
            if (std::ranges::find(checks, &state) == checks.end())
                checks.push_back(&state);
        }
    }
    void record_descriptor_resources(impl::GraphicResourceImpl& resource, const CommandBuffer& buffer, ResourceRetention retention) {
        const DescriptorRecording recording(resource);
        const auto completion = buffer.completion();
        resource.use.mark_used(completion);
        for (const auto& [binding, elements] : resource.bindings)
            for (const auto& element : elements) {
                for (const auto& owner : element.owners)
                    if (retention == ResourceRetention::COMPLETION)
                        buffer.retain_until_complete(detail::OwnerAccess::Shared(owner));
                    else
                        buffer.retain(detail::OwnerAccess::Shared(owner));
                for (const auto* state : element.states)
                    record_resource_state(*state, buffer, completion);
                for (const auto& owner : element.constraint.owners)
                    if (retention == ResourceRetention::COMPLETION)
                        buffer.retain_until_complete(detail::OwnerAccess::Shared(owner));
                    else
                        buffer.retain(detail::OwnerAccess::Shared(owner));
                for (const auto* state : element.constraint.states)
                    record_resource_state(*state, buffer, completion);
            }
        require_recording(buffer, completion);
        require_descriptor_valid(resource);
    }
    void release_completed_resources(impl::SubmissionWork& submission) noexcept {
        submission.release_resources = nullptr;
        if (submission.external_fence)
            submission.fence = nullptr;
        submission.external_fence = nullptr;
        submission.wait_semaphore = nullptr;
        submission.signal_semaphore = nullptr;
        submission.synchronization_owners.fill(impl::SubmissionWork::NO_OWNER);
        submission.native_buffer = nullptr;
        submission.acquisition = nullptr;
        submission.validity_checks.clear();
        auto buffer_owner = std::move(submission.unresolved_buffer_owner);
        submission.completion_resources.clear();
    }
} // namespace VSL_NAMESPACE::lifetime

namespace VSL_NAMESPACE {
    void Completion::retain(const std::shared_ptr<void>& resource) const {
        auto work = data_ ? data_->work.lock() : nullptr;
        if (!work || !sync::recordable(*data_))
            throw std::logic_error("Resources must be retained before submission");
        lifetime::retain_resource(work->resources, resource);
    }
    void Completion::retain(std::shared_ptr<void>&& resource) const {
        auto accepted = std::move(resource);
        auto work = data_ ? data_->work.lock() : nullptr;
        if (!work || !sync::recordable(*data_))
            throw std::logic_error("Resources must be retained before submission");
        lifetime::retain_resource(work->resources, std::move(accepted));
    }
    void CommandBuffer::retain(const std::shared_ptr<void>& resource) const {
        if (!data_ || !data_->recording_context)
            throw std::invalid_argument("CommandBuffer is empty");
        if (resource.use_count() && data_->recording_context->work) {
            if (data_->recording_context->submitting || !sync::recordable(*data_->recording_context->work->result))
                throw std::logic_error("Resources must be retained before submission");
            lifetime::retain_resource(data_->recording_context->work->resources, resource);
        }
    }
    void CommandBuffer::retain(std::shared_ptr<void>&& resource) const {
        auto accepted = std::move(resource);
        if (!data_ || !data_->recording_context)
            throw std::invalid_argument("CommandBuffer is empty");
        if (accepted.use_count() && data_->recording_context->work) {
            if (data_->recording_context->submitting || !sync::recordable(*data_->recording_context->work->result))
                throw std::logic_error("Resources must be retained before submission");
            lifetime::retain_resource(data_->recording_context->work->resources, std::move(accepted));
        }
    }
    void CommandBuffer::retain_until_complete(const std::shared_ptr<void>& resource) const {
        if (!data_ || !data_->recording_context || data_->recording_context->submitting || !data_->recording_context->work || !sync::recordable(*data_->recording_context->work->result))
            throw std::logic_error("Buffer retention requires a recording phase");
        lifetime::retain_until_complete(*data_->recording_context->work, resource);
    }
    void CommandBuffer::retain_until_complete(std::shared_ptr<void>&& resource) const {
        auto accepted = std::move(resource);
        if (!data_ || !data_->recording_context || data_->recording_context->submitting || !data_->recording_context->work || !sync::recordable(*data_->recording_context->work->result))
            throw std::logic_error("Buffer retention requires a recording phase");
        lifetime::retain_until_complete(*data_->recording_context->work, std::move(accepted));
    }
    impl::SubmissionWork::~SubmissionWork() {
        sync::finish_submission(*this);
    }
} // namespace VSL_NAMESPACE
