#include <VSL/lifetime/synchronization_access.hpp>
#include <VSL/lifetime/command_access.hpp>
#include <VSL/vulkan/_pimpls.hpp>
#include <VSL/vulkan/phase.hpp>
#include <VSL/sync/submission_completion.hpp>
#include <VSL/lifetime/submission_retention.hpp>
#include <VSL/lifetime/swapchain_storage.hpp>
#include <VSL/lifetime/presentation_retention.hpp>
#include <VSL/lifetime/acquire_ticket.hpp>
#include <VSL/lifetime/recovery_batch.hpp>
#include <VSL/lifetime/phase_call.hpp>
#include <VSL/lifetime/command_domain.hpp>
#include <VSL/lifetime/command_storage.hpp>
#include <VSL/lifetime/synchronization_use.hpp>
#include <VSL/sync/synchronization_state.hpp>
#include <VSL/sync/resource_use_access.hpp>
#include <VSL/sync/device_call.hpp>
#include <VSL/operation/submission_ops.hpp>
#include <VSL/operation/command_ops.hpp>
#include <VSL/operation/presentation_ops.hpp>
#include <VSL/operation/synchronization_ops.hpp>
#include <exception>
#include <stdexcept>
#include <algorithm>

namespace {
    using VSL_NAMESPACE::lifetime::detail::SynchronizationAccess;
}

namespace {
    using VSL_NAMESPACE::lifetime::CommandAccess;
}
namespace VSL_NAMESPACE {
    namespace {
        struct ReturningImage {
            lifetime::CommandRecordingContext& context;
            explicit ReturningImage(lifetime::CommandRecordingContext& context);
            ~ReturningImage() { context.returning_image = false; }
        };
        struct AbandonedAcquisitionRecovery {
            lifetime::AcquireTicket& ticket;
            ~AbandonedAcquisitionRecovery() {
                if (ticket.state.abandon_requested)
                    lifetime::collect_acquire_scope(ticket, true);
            }
        };
        template<typename T>
        std::shared_ptr<T> synchronization_owner(const impl::SubmissionWork& work, impl::SubmissionSource source, T* value);
        size_t retain_synchronization_owner(impl::SubmissionWork& work, const std::shared_ptr<void>& owner);

        template<typename T>
        std::shared_ptr<T> synchronization_owner(const impl::SubmissionWork& work, impl::SubmissionSource source, T* value) {
            if (!value)
                return {};
            return {work.completion_resources.at(work.synchronization_owners.at(static_cast<size_t>(source))), value};
        }
        size_t retain_synchronization_owner(impl::SubmissionWork& work, const std::shared_ptr<void>& owner) {
            if (!owner.use_count())
                return impl::SubmissionWork::NO_OWNER;
            lifetime::retain_until_complete(work, owner);
            const auto found = std::ranges::find_if(work.completion_resources, [&](const auto& existing) {
                return !existing.owner_before(owner) && !owner.owner_before(existing);
            });
            return static_cast<size_t>(found - work.completion_resources.begin());
        }
        ReturningImage::ReturningImage(lifetime::CommandRecordingContext& context) : context(context) {
            if (context.returning_image)
                throw std::logic_error("Acquired image is already being returned");
            context.returning_image = true;
        }
    } // namespace
    ComputePhase::ComputePhase(CommandManager manager,
                               std::optional<SemaphoreHolder> available,
                               std::optional<SemaphoreHolder> finished,
                               std::optional<FenceHolder> fence, bool postpone,
                               PipelineStage stage)
        : manager(manager), next_image_available(available), calculation_finish(finished),
          in_flight_fence(fence), wait_stage(stage), initial_exceptions(std::uncaught_exceptions()) {
        if (!postpone)
            setup();
    }

    bool ComputePhase::prepare(bool returning_image) {
        lifetime::PhaseCall call(*this);
        recording_buffer = manager.get_buffer();
        const auto domain = lifetime::CommandAccess::Owner(recording_buffer);
        auto buffers = domain->buffer;
        if (domain->context.returning_image && !returning_image)
            throw std::logic_error("Acquired image is being returned");
        if (domain->context.closing || domain->context.recording || domain->context.submitting || domain->context.mutating ||
            (!returning_image && domain->context.frame_buffer_recording))
            throw std::logic_error("Command domain is unavailable for recording");
        const auto generation = domain->context.generation;
        const auto manager_data = CommandAccess::Data(manager);
        const auto inherited = returning_image ? submission : nullptr;
        const auto available = inherited ? synchronization_owner(*inherited, impl::SubmissionSource::WAIT_SEMAPHORE, inherited->wait_semaphore)
                                         : (next_image_available ? SynchronizationAccess::Owned(*next_image_available).share() : nullptr);
        const auto finished = inherited ? synchronization_owner(*inherited, impl::SubmissionSource::SIGNAL_SEMAPHORE, inherited->signal_semaphore)
                                        : (calculation_finish ? SynchronizationAccess::Owned(*calculation_finish).share() : nullptr);
        const auto external = inherited ? synchronization_owner(*inherited, impl::SubmissionSource::FENCE, inherited->external_fence)
                                        : (in_flight_fence ? SynchronizationAccess::Owned(*in_flight_fence).share() : nullptr);
        if ((!inherited && next_image_available && !available) || (available && available->device != manager_data->device.get()))
            throw std::invalid_argument("Acquire semaphore device mismatch");
        if ((!inherited && calculation_finish && !finished) || (finished && finished->device != manager_data->device.get()))
            throw std::invalid_argument("Signal semaphore device mismatch");
        if ((!inherited && in_flight_fence && !external) || (external && external->device != manager_data->device.get()))
            throw std::invalid_argument("Completion fence device mismatch");
        frame_slot = buffers->current_buffer_idx;
        signal_index = frame_slot;
        auto device = manager_data->device->device;
        auto previous = domain->submissions.at(frame_slot);
        if (!inherited && previous && sync::pending_use(*previous->result)) {
            const auto busy = Completion{previous->result}.busy();
            if (!call.alive())
                return false;
            if (busy) {
                if (previous->result->status != Completion::Status::SUBMITTED || previous->result->native.active)
                    throw std::logic_error("Previous native submission is unresolved");
                const auto completed = Completion{previous->result}.wait();
                if (!call.alive())
                    return false;
                if (!completed)
                    throw std::runtime_error("Previous frame completion failed");
            }
        }
        if (available && available->semaphores.size() <= frame_slot)
            throw std::invalid_argument("Acquire semaphore count must cover frame slots");
        if (external && external->fences.size() <= frame_slot)
            throw std::invalid_argument("Fence count must cover frame slots");
        auto next = std::make_shared<impl::SubmissionWork>();
        next->device = manager_data->device;
        next->native_buffer = buffers.get();
        next->frame_slot = frame_slot;
        next->release_resources = lifetime::release_completed_resources;
        next->wait_semaphore = available.get();
        next->signal_semaphore = finished.get();
        next->synchronization_owners[static_cast<size_t>(impl::SubmissionSource::WAIT_SEMAPHORE)] = retain_synchronization_owner(*next, available);
        next->synchronization_owners[static_cast<size_t>(impl::SubmissionSource::SIGNAL_SEMAPHORE)] = retain_synchronization_owner(*next, finished);
        next->synchronization_owners[static_cast<size_t>(impl::SubmissionSource::FENCE)] = retain_synchronization_owner(*next, external);
        if (available) {
            lifetime::require_semaphore_owner(available);
            sync::track_semaphore(*available, next->result);
        }
        if (finished) {
            lifetime::require_semaphore_owner(finished);
            sync::track_semaphore(*finished, next->result);
        }
        if (inherited) {
            lifetime::require_fence_reservation(*inherited, frame_slot);
            next->external_fence = external.get();
            next->owned_fence = inherited->owned_fence;
            next->fence = inherited->fence;
            next->fence_reservation = inherited->fence_reservation;
        } else if (external) {
            next->external_fence = external.get();
            next->fence = external->fences.at(frame_slot);
            next->fence_reservation = lifetime::reserve_fence(external, frame_slot, next->result);
            if (!call.alive())
                return false;
        } else {
            next->owned_fence = previous ? previous->owned_fence : nullptr;
            if (!next->owned_fence)
                next->owned_fence = std::make_shared<vk::UniqueFence>(device.createFenceUnique({}));
            next->fence = **next->owned_fence;
        }
        if (domain->context.closing || domain->context.recording || domain->context.submitting || domain->context.generation != generation ||
            CommandAccess::Data(manager) != manager_data || manager_data->command_buffer != buffers ||
            buffers->current_buffer_idx != frame_slot || domain->submissions.at(frame_slot) != previous)
            throw std::logic_error("Frame slot changed while preparing");
        next->result->work = next;
        if (inherited && next->fence_reservation)
            next->fence_reservation->result = next->result;
        submission = std::move(next);
        completion_ = Completion{submission->result};
        domain->context.acquired_image_index.reset();
        domain->context.acquired_swapchain.reset();
        return true;
    }

    void ComputePhase::fail(Completion::Failure failure, std::int32_t error_code) noexcept {
        const auto buffer = recording_buffer;
        const auto work = submission;
        if (work && work->result->native.active) {
            work->abandon_requested = true;
            return;
        }
        const auto slot = work ? work->frame_slot : frame_slot;
        state = State::ERROR_;
        if (!work)
            return;
        const auto domain = CommandAccess::Data(buffer) && CommandAccess::Data(buffer)->owner_context ? CommandAccess::Data(buffer)->owner_context->domain.lock() : nullptr;
        if (domain)
            lifetime::fail_recording(*domain, *work, slot, failure, error_code);
        else {
            if (failure == Completion::Failure::DEVICE_LOST)
                sync::save_native_idle(*work, static_cast<std::int32_t>(vk::Result::eErrorDeviceLost));
            sync::publish_terminal(*work, Completion::Status::FAILED, failure, error_code);
        }
    }
    void ComputePhase::begin() {
        lifetime::PhaseCall call(*this);
        const auto domain = lifetime::CommandAccess::Owner(recording_buffer);
        const auto buffers = domain->buffer;
        const auto work = submission;
        const auto slot = work->frame_slot;
        const auto fail_local = [&](Completion::Failure failure, std::int32_t error = 0) {
            if (call.alive())
                state = State::ERROR_;
            lifetime::fail_recording(*domain, *work, slot, failure, error);
        };
        try {
            if (domain->context.closing || domain->context.recording || domain->context.submitting || CommandAccess::Data(manager)->command_buffer != buffers ||
                frame_slot != slot || buffers->current_buffer_idx != slot ||
                (domain->submissions.at(slot) && sync::pending_use(*domain->submissions.at(slot)->result)))
                throw std::logic_error("Frame slot is no longer available for recording");
            domain->recording_submission = work;
            domain->context.work = work.get();
            lifetime::SubmissionTransaction transaction(*domain, *work);
            operation::reset_command_buffer(buffers->command_buffers.at(slot));
            if (call.alive() && !work->abandon_requested)
                operation::begin_command_buffer(buffers->command_buffers.at(slot));
            if (!call.alive() || work->abandon_requested) {
                lifetime::abort_recording(*domain, *work, slot);
                sync::save_terminal(*work, Completion::Status::CANCELLED);
                if (call.alive())
                    state = State::ERROR_;
                transaction.finish();
                sync::finish_submission(*work);
                return;
            }
            domain->context.recording = true;
            ++domain->context.generation;
            state = State::WAIT_SUBMIT;
            transaction.finish();
        } catch (const vk::SystemError& error) {
            const auto failure = error.code().value() == static_cast<int>(vk::Result::eErrorDeviceLost) ? Completion::Failure::DEVICE_LOST : Completion::Failure::SUBMIT_FAILED;
            fail_local(failure, error.code().value());
            throw;
        } catch (...) {
            fail_local(Completion::Failure::SUBMIT_FAILED);
            throw;
        }
    }
    void ComputePhase::setup() {
        lifetime::PhaseCall call(*this);
        const lifetime::SurfaceRetirementCollection retirement(submission ? submission->device : (CommandAccess::Data(manager) ? CommandAccess::Data(manager)->device : nullptr));
        if (state != State::BEFORE_SETUP)
            return;
        try {
            if (!prepare() || !call.alive())
                return;
            begin();
        } catch (const vk::SystemError& error) {
            if (call.alive())
                fail(error.code().value() == static_cast<int>(vk::Result::eErrorDeviceLost) ? Completion::Failure::DEVICE_LOST : Completion::Failure::SUBMIT_FAILED, error.code().value());
            throw;
        } catch (...) {
            if (call.alive())
                fail(Completion::Failure::SUBMIT_FAILED);
            throw;
        }
    }
    ComputePhase::SubmitResult ComputePhase::submit() {
        lifetime::PhaseCall call(*this);
        const lifetime::SurfaceRetirementCollection retirement(submission ? submission->device : (CommandAccess::Data(manager) ? CommandAccess::Data(manager)->device : nullptr));
        if (state != State::WAIT_SUBMIT)
            return SubmitResult::FAILED;
        if (submission && submission->result->native.active) {
            submission->abandon_requested = true;
            return SubmitResult::FAILED;
        }
        const auto work = submission;
        const auto domain = lifetime::CommandAccess::Owner(recording_buffer);
        const auto buffers = domain->buffer;
        const auto manager_data = CommandAccess::Data(manager);
        const auto slot = work->frame_slot;
        const auto acquisition = synchronization_owner(*work, impl::SubmissionSource::ACQUISITION, work->acquisition);
        const auto signal_slot = signal_index;
        const auto stage = wait_stage;
        std::shared_ptr<impl::SubmissionWork> previous;
        const auto fail_local = [&](Completion::Failure failure, std::int32_t error_code = 0) {
            if (work->result->status != Completion::Status::PENDING)
                return;
            if (call.alive())
                state = State::ERROR_;
            if (acquisition)
                lifetime::request_acquire_abandon(*acquisition);
            lifetime::fail_recording(*domain, *work, slot, failure, error_code);
            if (acquisition)
                lifetime::collect_acquire_scope(*acquisition, true);
        };
        try {
            if (domain->context.closing || domain->context.submitting || !domain->context.recording || domain->context.work != work.get() ||
                !sync::recordable(*work->result) || frame_slot != slot || slot >= domain->submissions.size() ||
                manager_data->command_buffer != buffers || buffers->current_buffer_idx != slot)
                throw std::logic_error("Frame slot changed while recording");
            lifetime::require_fence_reservation(*work, slot);
            if (!domain->context.returning_image &&
                ((next_image_available ? SynchronizationAccess::Owned(*(next_image_available)).get() : nullptr) != work->wait_semaphore ||
                 (calculation_finish ? SynchronizationAccess::Owned(*(calculation_finish)).get() : nullptr) != work->signal_semaphore ||
                 (in_flight_fence ? SynchronizationAccess::Owned(*(in_flight_fence)).get() : nullptr) != work->external_fence))
                throw std::logic_error("Phase synchronization owners changed after preparation");
            if (buffers->owner_context->unresolved_submissions == std::numeric_limits<size_t>::max())
                throw std::overflow_error("Command ring uncertainty count overflow");
            std::vector<vk::Semaphore> waits, signals;
            if (work->wait_semaphore) {
                sync::require_graphics_wait(*work->wait_semaphore, slot, work->result,
                                            work->acquisition ? work->acquisition->available_use.get() : nullptr);
                waits.push_back(work->wait_semaphore->semaphores.at(slot));
            }
            if (work->signal_semaphore) {
                sync::require_graphics_signal(*work->signal_semaphore, signal_slot, work->result,
                                              work->acquisition ? work->acquisition->finished_use.get() : nullptr);
                signals.push_back(work->signal_semaphore->semaphores.at(signal_slot));
            }
            auto stages = vk::PipelineStageFlags(static_cast<std::uint32_t>(stage));
            if (!stages && !waits.empty())
                throw std::invalid_argument("Semaphore wait stage cannot be empty");
            for (const auto* check : work->validity_checks)
                sync::ResourceUseAccess::RequireKnown(*check);
            const auto command = buffers->command_buffers.at(slot);
            previous = domain->submissions.at(slot);
            work->wait_index = slot;
            work->signal_index = signal_slot;
            vk::SubmitInfo info{
                .waitSemaphoreCount = static_cast<std::uint32_t>(waits.size()), .pWaitSemaphores = waits.data(), .pWaitDstStageMask = &stages, .commandBufferCount = 1, .pCommandBuffers = &command, .signalSemaphoreCount = static_cast<std::uint32_t>(signals.size()), .pSignalSemaphores = signals.data()};
            vk::Result result;
            {
                lifetime::SubmissionTransaction transaction(*domain, *work);
                operation::end_command_buffer(command);
                operation::reset_fences(work->device->device, {&work->fence, 1});
                for (const auto* check : work->validity_checks)
                    sync::ResourceUseAccess::RequireKnown(*check);
                result = work->device->lost ? vk::Result::eErrorDeviceLost : operation::queue_submit(work->device->device, manager_data->graphics_queue, info, work->fence);
                if (work->device->lost)
                    result = vk::Result::eErrorDeviceLost;
                sync::save_submit_result(*work->result, static_cast<std::int32_t>(result));
                if (result == vk::Result::eSuccess)
                    work->result->status = Completion::Status::SUBMITTED;
                else {
                    if (result == vk::Result::eErrorDeviceLost)
                        work->device->lost = true;
                    sync::save_terminal(*work, Completion::Status::FAILED,
                                        result == vk::Result::eErrorDeviceLost ? Completion::Failure::DEVICE_LOST : Completion::Failure::SUBMIT_FAILED,
                                        static_cast<std::int32_t>(result));
                }
                if (work->result->native.enqueue == sync::SubmissionEnqueue::UNKNOWN) {
                    if (work->wait_semaphore)
                        sync::poison_semaphore_element(*work->wait_semaphore, slot);
                    if (work->signal_semaphore)
                        sync::poison_semaphore_element(*work->signal_semaphore, signal_slot);
                    lifetime::quarantine_submission(work, buffers);
                }
                if (result == vk::Result::eSuccess || work->result->native.enqueue == sync::SubmissionEnqueue::UNKNOWN) {
                    domain->submissions.at(slot) = work;
                    domain->context.recording = false;
                    domain->context.work = nullptr;
                    domain->recording_submission.reset();
                    if (work->external_fence)
                        sync::mark_fence_submitted(*work->external_fence, slot, work->result);
                } else
                    lifetime::abort_recording(*domain, *work, slot);
                if (call.alive())
                    state = result == vk::Result::eSuccess ? State::FINISH_SUBMIT : State::ERROR_;
                transaction.finish();
            }
            if (result != vk::Result::eSuccess) {
                if (acquisition) {
                    lifetime::request_acquire_abandon(*acquisition);
                    lifetime::collect_acquire_scope(*acquisition, true);
                }
                sync::finish_submission(*work);
                vk::detail::throwResultException(result, "Graphics submission failed");
            }
            if (acquisition) {
                if (work->abandon_requested)
                    lifetime::request_acquire_abandon(*acquisition);
                if (acquisition->state.abandon_requested)
                    lifetime::collect_acquire_scope(*acquisition, true);
            }
        } catch (const vk::SystemError& error) {
            const auto failure = error.code().value() == static_cast<int>(vk::Result::eErrorDeviceLost) ? Completion::Failure::DEVICE_LOST : Completion::Failure::SUBMIT_FAILED;
            fail_local(failure, error.code().value());
            throw;
        } catch (...) {
            fail_local(Completion::Failure::SUBMIT_FAILED);
            throw;
        }
        return SubmitResult::SUCCESS;
    }
    Completion::Failure ComputePhase::cancel_recording(std::int32_t& error_code) {
        lifetime::PhaseCall call(*this);
        const auto buffer = recording_buffer;
        const auto work = submission;
        const auto buffers = CommandAccess::Data(buffer);
        auto* context = buffers ? buffers->recording_context : nullptr;
        Completion::Failure failure = Completion::Failure::NONE;
        if (work && context && context->work == work.get()) {
            const auto domain = lifetime::CommandAccess::Owner(buffer);
            lifetime::SubmissionTransaction transaction(*domain, *work);
            try {
                operation::reset_command_buffer(buffers->command_buffers.at(work->frame_slot));
            } catch (const vk::SystemError& error) {
                error_code = error.code().value();
                failure = error_code == static_cast<int>(vk::Result::eErrorDeviceLost) ? Completion::Failure::DEVICE_LOST : Completion::Failure::SUBMIT_FAILED;
            } catch (...) {
                failure = Completion::Failure::SUBMIT_FAILED;
            }
            context->recording = false;
            context->work = nullptr;
            domain->recording_submission.reset();
            if (failure == Completion::Failure::DEVICE_LOST)
                sync::save_native_idle(*work, static_cast<std::int32_t>(vk::Result::eErrorDeviceLost));
            transaction.finish();
        }
        if (call.alive())
            state = State::ERROR_;
        return failure;
    }
    void ComputePhase::cancel() {
        if (submission && submission->result->native.active) {
            submission->abandon_requested = true;
            return;
        }
        if (state != State::WAIT_SUBMIT && state != State::BEFORE_SETUP)
            return;
        const auto buffer = recording_buffer;
        const auto work = submission;
        std::int32_t error_code = 0;
        const auto failure = cancel_recording(error_code);
        if (work)
            sync::publish_terminal(*work, failure == Completion::Failure::NONE ? Completion::Status::CANCELLED : Completion::Status::FAILED, failure, error_code);
    }
    void ComputePhase::finish_noexcept() noexcept {
        if (submission && submission->result->native.active) {
            submission->abandon_requested = true;
            return;
        }
        try {
            if (std::uncaught_exceptions() > initial_exceptions)
                cancel();
            else
                submit();
        } catch (...) {
            ComputePhase::cancel();
            loggingln("Phase submission failed during destruction");
        }
    }
    ComputePhase::~ComputePhase() {
        lifetime::PhaseCall::Invalidate(*this);
        finish_noexcept();
    }
    std::uint32_t ComputePhase::get_image_index() {
        return image_index;
    }

    DefaultPhase::DefaultPhase(CommandManager manager, SwapchainAccessor chain,
                               std::optional<SemaphoreHolder> available,
                               std::optional<SemaphoreHolder> finished,
                               std::optional<FenceHolder> fence, bool postpone, std::optional<FenceHolder> acquire_fences)
        : ComputePhase(manager, available, finished, fence, true), swapchain(chain), _acquire_fences(std::move(acquire_fences)) {
        if (!postpone)
            setup();
    }
    void DefaultPhase::setup() {
        lifetime::PhaseCall call(*this);
        const lifetime::SurfaceRetirementCollection retirement(submission ? submission->device : (CommandAccess::Data(manager) ? CommandAccess::Data(manager)->device : nullptr));
        if (state != State::BEFORE_SETUP)
            return;
        if (_setting_up)
            throw std::logic_error("Phase setup is already in progress");
        _setting_up = true;
        struct SetupGuard {
            lifetime::PhaseCall& call;
            bool& active;
            ~SetupGuard() {
                if (call.alive())
                    active = false;
            }
        } guard{call, _setting_up};
        std::shared_ptr<lifetime::AcquireTicket> presentation;
        std::shared_ptr<lifetime::CommandDomain> owner_domain;
        try {
            const auto chain = swapchain.data_;
            if (!chain || !chain->swap_chain || !CommandAccess::Data(manager) ||
                chain->device != CommandAccess::Data(manager)->device.get())
                throw std::invalid_argument("A swapchain is required");
            lifetime::require_swapchain_owner(chain);
            const auto buffer = manager.get_buffer();
            owner_domain = lifetime::CommandAccess::Owner(buffer);
            const auto& domain = owner_domain;
            auto buffers = domain->buffer;
            const auto initial_manager = CommandAccess::Data(manager);
            if (domain->context.closing || domain->context.recording || domain->context.submitting || domain->context.returning_image || domain->context.mutating || domain->context.frame_buffer_recording)
                throw std::logic_error("A phase is already recording");
            const bool initially_ready = acquisition_ready();
            if (!call.alive() || state != State::BEFORE_SETUP)
                return;
            if (CommandAccess::Data(manager) != initial_manager || initial_manager->command_buffer != buffers || domain->buffer != buffers || swapchain.data_ != chain ||
                domain->context.closing || domain->context.recording || domain->context.submitting || domain->context.returning_image || domain->context.mutating || domain->context.frame_buffer_recording)
                throw std::logic_error("Command domain changed during acquisition readiness");
            if (!initially_ready) {
                state = State::SUSPENDED;
                return;
            }
            operation::require_active_swapchain(*chain);
            if (!calculation_finish)
                calculation_finish = SynchronizationAccess::FromShared(lifetime::swapchain_finished(chain));
            const auto previous_slot = buffers->current_buffer_idx;
            buffers->current_buffer_idx = (previous_slot + 1) % buffers->command_buffers.size();
            if (!prepare() || !call.alive())
                return;
            if (swapchain.data_ != chain)
                throw std::logic_error("Swapchain changed while preparing a phase");
            operation::require_active_swapchain(*chain);
            const auto count = chain->swap_chain_images.size();
            if (submission->signal_semaphore && submission->signal_semaphore->semaphores.size() < count)
                throw std::invalid_argument("Present semaphores must cover swapchain images");
            const auto work = submission;
            const auto manager_data = CommandAccess::Data(manager);
            const auto slot = frame_slot;
            const auto prepared_generation = domain->context.generation;
            const auto previous = domain->submissions.at(slot);
            bool cancelled = false;
            const auto require_prepared = [&, this] {
                if (!call.alive())
                    return false;
                if (work->abandon_requested) {
                    cancelled = true;
                    if (presentation)
                        lifetime::request_acquire_abandon(*presentation);
                    ComputePhase::cancel();
                    return false;
                }
                if (submission != work || work->result->status != Completion::Status::PENDING ||
                    swapchain.data_ != chain || CommandAccess::Data(manager) != manager_data || manager_data->command_buffer != buffers ||
                    domain->buffer != buffers || buffers->current_buffer_idx != slot || frame_slot != slot ||
                    domain->submissions.at(slot) != previous || domain->context.generation != prepared_generation ||
                    domain->context.closing || domain->context.recording || domain->context.submitting || domain->context.returning_image ||
                    domain->context.mutating || domain->context.frame_buffer_recording)
                    throw std::logic_error("Frame slot changed while preparing acquisition");
                operation::require_active_swapchain(*chain);
                return true;
            };
            auto ready = [&] {
                if (!require_prepared())
                    return false;
                const bool result = acquisition_ready();
                return require_prepared() && result;
            };
            const auto suspend = [&] {
                if (cancelled)
                    return;
                if (presentation)
                    presentation->reject_acquisition();
                sync::save_terminal(*work, Completion::Status::CANCELLED);
                if (call.alive()) {
                    buffers->current_buffer_idx = previous_slot;
                    state = State::SUSPENDED;
                    _presentation.reset();
                    submission.reset();
                }
                sync::finish_submission(*work);
                if (presentation) {
                    const auto registry = presentation->presentations.lock();
                    lifetime::release_acquire_owners(*presentation);
                    if (registry && presentation->released) {
                        std::erase(registry->tickets, presentation);
                        std::erase(registry->acquisitions, presentation->evidence);
                    }
                }
            };
            if (!ready()) {
                suspend();
                return;
            }
            vk::Semaphore available;
            const auto registry = lifetime::presentation_domain(chain);
            work->surface = registry->surface;
            work->surface_address = chain->surface;
            lifetime::register_presentation_domain(*domain, registry);
            if (work->wait_semaphore)
                available = work->wait_semaphore->semaphores.at(slot);
            if (work->signal_semaphore)
                for (std::uint32_t i = 0; i < count; ++i)
                    sync::require_semaphore_element(*work->signal_semaphore, i);
            const auto source = _acquire_fences ? SynchronizationAccess::Owned(*_acquire_fences).share()
                                                : lifetime::acquire_fences(*domain, LogicalDeviceAccessor{manager_data->device});
            if (!require_prepared())
                return;
            SynchronizationAccess::RequireCanonical(source);
            if (source->device != manager_data->device.get() || source->fences.size() < buffers->command_buffers.size())
                throw std::invalid_argument("Acquisition fences must cover frame slots on the same device");
            const auto acquired = source->fences.at(slot);
            if (acquired == work->fence)
                throw std::invalid_argument("Acquisition and graphics cannot share a fence element");
            presentation = lifetime::prepare_acquire_ticket(registry, chain,
                                                            synchronization_owner(*work, impl::SubmissionSource::WAIT_SEMAPHORE, work->wait_semaphore),
                                                            synchronization_owner(*work, impl::SubmissionSource::SIGNAL_SEMAPHORE, work->signal_semaphore),
                                                            source, work->result, manager_data->graphics_queue, manager_data->present_queue, slot);
            _presentation = presentation;
            work->acquisition = presentation.get();
            work->synchronization_owners[static_cast<size_t>(impl::SubmissionSource::ACQUISITION)] = retain_synchronization_owner(*work, presentation);
            const lifetime::AcquireFenceGate gate{&ready, [](void* value) { return (*static_cast<decltype(ready)*>(value))(); }};
            if (!lifetime::reserve_acquire_fence(source, slot, {presentation->evidence, &presentation->evidence->proof}, gate) || !ready()) {
                suspend();
                return;
            }
            std::uint32_t acquired_index = UINT32_MAX;
            vk::Result result;
            {
                const lifetime::NativeCall native_call(presentation->state.native_call_active);
                const lifetime::NativeCall fence_call(source->uses.at(slot).accessing);
                result = chain->device->lost ? vk::Result::eErrorDeviceLost
                                             : operation::acquire_next_image(chain->native_device, chain->swap_chain, UINT64_MAX, available, acquired, acquired_index);
                if (chain->device->lost)
                    result = vk::Result::eErrorDeviceLost;
                lifetime::adopt_acquisition(*presentation, result, acquired_index);
            }
            if (presentation->state.status == sync::AcquisitionStatus::NOT_ACQUIRED)
                lifetime::release_acquire_fence(*source, slot, {presentation->evidence, &presentation->evidence->proof});
            if (!call.alive()) {
                lifetime::request_acquire_abandon(*presentation);
                sync::publish_terminal(*work, Completion::Status::CANCELLED);
                lifetime::collect_acquire_scope(*presentation, true);
                return;
            }
            if (result == vk::Result::eErrorOutOfDateKHR) {
                recreate = true;
                ComputePhase::cancel();
                return;
            }
            if (result != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR) {
                fail(result == vk::Result::eErrorDeviceLost ? Completion::Failure::DEVICE_LOST : Completion::Failure::SUBMIT_FAILED, static_cast<std::int32_t>(result));
                throw std::runtime_error("Swapchain acquire failed: " + vk::to_string(result));
            }
            image_index = acquired_index;
            recreate = result == vk::Result::eSuboptimalKHR;
            if (!available && !sync::wait_fence_proof(presentation->evidence->proof))
                throw std::runtime_error("Swapchain acquire wait failed");
            if (!require_prepared()) {
                lifetime::request_acquire_abandon(*presentation);
                lifetime::collect_acquire_scope(*presentation, true);
                return;
            }
            signal_index = presentation->acquired_index;
            begin();
            if (!call.alive() || work->result->status != Completion::Status::PENDING || domain->context.work != work.get()) {
                lifetime::request_acquire_abandon(*presentation);
                lifetime::collect_acquire_scope(*presentation, true);
                return;
            }
            domain->context.acquired_image_index = presentation->acquired_index;
            domain->context.acquired_swapchain = chain;
        } catch (const vk::SystemError& error) {
            if (presentation)
                lifetime::request_acquire_abandon(*presentation);
            if (call.alive())
                fail(error.code().value() == static_cast<int>(vk::Result::eErrorDeviceLost) ? Completion::Failure::DEVICE_LOST : Completion::Failure::SUBMIT_FAILED, error.code().value());
            if (presentation)
                lifetime::collect_acquire_scope(*presentation, true);
            throw;
        } catch (...) {
            if (presentation)
                lifetime::request_acquire_abandon(*presentation);
            if (call.alive())
                fail(Completion::Failure::SUBMIT_FAILED);
            if (presentation)
                lifetime::collect_acquire_scope(*presentation, true);
            throw;
        }
    }
    std::uint32_t DefaultPhase::get_current_index() {
        return manager.get_current_buffer_idx();
    }
    DefaultPhase::SubmitResult DefaultPhase::submit_commands() {
        if (state == State::ERROR_ && recreate)
            return SubmitResult::WANT_RECREATE_SWAPCHAIN;
        return ComputePhase::submit();
    }
    DefaultPhase::SubmitResult DefaultPhase::present() {
        lifetime::PhaseCall call(*this);
        const lifetime::SurfaceRetirementCollection retirement(submission ? submission->device : (CommandAccess::Data(manager) ? CommandAccess::Data(manager)->device : nullptr));
        if (submission && (submission->result->native.active || (_presentation && _presentation->state.native_call_active))) {
            submission->abandon_requested = true;
            if (_presentation)
                lifetime::request_acquire_abandon(*_presentation);
            return SubmitResult::FAILED;
        }
        if (state != State::FINISH_SUBMIT || _presented || !_presentation || !_presentation->state.acquired ||
            _presentation->released || _presentation->state.abandon_requested)
            return SubmitResult::FAILED;
        const auto presentation = _presentation;
        const auto work = submission;
        const auto domain = lifetime::CommandAccess::Owner(recording_buffer);
        const auto registry = presentation->presentations.lock();
        if (!registry || registry->closing || std::ranges::find(domain->presentation_domains, registry) == domain->presentation_domains.end())
            throw std::logic_error("Acquisition lost its original Surface domain");
        lifetime::require_presentable_acquisition(*presentation);
        const auto generation = presentation->generation();
        const auto queue = presentation->present_queue;
        const auto want_recreate = recreate;
        sync::PresentEnqueue outcome;
        vk::Result native;
        {
            const lifetime::NativeCall native_call(presentation->state.native_call_active);
            if (!presentation->record) {
                lifetime::register_presentation_domain(*domain, registry);
                presentation->record = lifetime::prepare_present_record(*registry, generation, presentation->finished_owner(),
                                                                        presentation->finished_use, queue, presentation->acquired_index);
            }
            outcome = lifetime::enqueue_present_record(*presentation->record);
            native = presentation->record->state.result;
            lifetime::delegate_acquisition(*presentation, presentation->record);
            if (call.alive())
                _presented = outcome != sync::PresentEnqueue::NOT_ENQUEUED;
        }
        auto result = SubmitResult::FAILED;
        if (outcome == sync::PresentEnqueue::ENQUEUED) {
            if (want_recreate || native == vk::Result::eSuboptimalKHR || native == vk::Result::eErrorOutOfDateKHR)
                result = SubmitResult::WANT_RECREATE_SWAPCHAIN;
            else if (native == vk::Result::eSuccess)
                result = SubmitResult::SUCCESS;
        }
        lifetime::collect_acquire_scope(*presentation);
        return result; // callbackがPhaseを破棄しても、以降thisを参照しない。
    }
    DefaultPhase::SubmitResult DefaultPhase::submit() {
        lifetime::PhaseCall call(*this);
        if (submission && (submission->result->native.active || (_presentation && _presentation->state.native_call_active))) {
            submission->abandon_requested = true;
            if (_presentation)
                lifetime::request_acquire_abandon(*_presentation);
            return SubmitResult::FAILED;
        }
        if (state == State::ERROR_ && recreate)
            return SubmitResult::WANT_RECREATE_SWAPCHAIN;
        const auto domain = CommandAccess::Data(recording_buffer) ? lifetime::CommandAccess::Owner(recording_buffer) : nullptr;
        const auto slot = submission ? submission->frame_slot : frame_slot;
        const auto previous = domain && slot < domain->submissions.size() ? domain->submissions.at(slot) : nullptr;
        if (state == State::WAIT_SUBMIT) {
            const auto result = submit_commands();
            if (!call.alive())
                return result;
            if (result != SubmitResult::SUCCESS)
                return SubmitResult::FAILED;
        }
        return present();
    }
    void DefaultPhase::cancel() {
        lifetime::PhaseCall call(*this);
        if (submission && (submission->result->native.active || (_presentation && _presentation->state.native_call_active))) {
            submission->abandon_requested = true;
            if (_presentation)
                lifetime::request_acquire_abandon(*_presentation);
            return;
        }
        const auto work = submission;
        if (work && work->device && sync::NativeDeviceCall::Active(work->device->device)) {
            work->abandon_requested = true;
            const auto presentation = _presentation;
            if (presentation)
                lifetime::request_acquire_abandon(*presentation);
            if (work->result->status == Completion::Status::PENDING && !sync::native_pending(*work->result)) {
                if (CommandAccess::Data(recording_buffer))
                    lifetime::detach_recording_cpu(*lifetime::CommandAccess::Owner(recording_buffer), *work);
                state = State::ERROR_;
                if (presentation && !presentation->state.acquired)
                    presentation->reject_acquisition();
                sync::save_terminal(*work, Completion::Status::CANCELLED);
                sync::finish_submission(*work);
            } else
                state = State::ERROR_;
            return;
        }
        if (!_presentation || !_presentation->state.acquired || _presented) {
            ComputePhase::cancel();
            return;
        }
        const auto presentation = _presentation;
        const AbandonedAcquisitionRecovery recovery{*presentation};
        if (state == State::WAIT_SUBMIT) {
            const auto index = presentation->acquired_index;
            const auto generation = presentation->generation();
            const auto domain = lifetime::CommandAccess::Owner(recording_buffer);
            const ReturningImage returning(domain->context);
            const auto cancelled = submission;
            const auto previous = domain->submissions.at(frame_slot);
            std::int32_t error_code = 0;
            const auto failure = cancel_recording(error_code);
            if (!call.alive()) {
                lifetime::request_acquire_abandon(*presentation);
                sync::publish_terminal(*cancelled, failure == Completion::Failure::NONE ? Completion::Status::CANCELLED : Completion::Status::FAILED, failure, error_code);
                return;
            }
            if (failure != Completion::Failure::NONE) {
                lifetime::request_acquire_abandon(*presentation);
                sync::publish_terminal(*cancelled, Completion::Status::FAILED, failure, error_code);
                return;
            }
            // 途中の記録を破棄し、取得時のセマフォを消費して画像を画面表示に戻す。
            // 元結果のcallbackは返却の記録・送信後に呼び、Phase自己破棄後にthisへ触れない。
            try {
                if (!prepare(true) || !call.alive())
                    return;
                lifetime::transfer_acquire_graphics(*presentation, submission->result);
                submission->acquisition = presentation.get();
                submission->synchronization_owners[static_cast<size_t>(impl::SubmissionSource::ACQUISITION)] = retain_synchronization_owner(*submission, presentation);
                signal_index = index;
                begin();
                if (!call.alive()) {
                    sync::publish_terminal(*cancelled, Completion::Status::CANCELLED);
                    return;
                }
                if (!submission || submission->result->status != Completion::Status::PENDING || domain->context.work != submission.get() || !domain->context.recording) {
                    sync::publish_terminal(*cancelled, Completion::Status::CANCELLED);
                    return;
                }
                auto buffers = domain->buffer;
                domain->context.acquired_image_index = index;
                domain->context.acquired_swapchain = generation;
                const auto image = lifetime::swapchain_image(generation, index);
                sync::ResourceUseAccess::TrackPending(*image->use, completion_);
                vk::ImageMemoryBarrier barrier{
                    .oldLayout = vk::ImageLayout::eUndefined, .newLayout = vk::ImageLayout::ePresentSrcKHR, .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED, .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED, .image = generation->swap_chain_images.at(index), .subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}};
                buffers->command_buffers.at(frame_slot).pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eBottomOfPipe, {}, {}, {}, barrier);
                if (!call.alive()) {
                    sync::publish_terminal(*cancelled, Completion::Status::CANCELLED);
                    return;
                }
                DefaultPhase::submit_commands(); // キャンセル時は派生クラスの終了コールバックを呼ばない。
                if (!call.alive()) {
                    sync::publish_terminal(*cancelled, Completion::Status::CANCELLED);
                    return;
                }
                if (state == State::FINISH_SUBMIT)
                    present();
            } catch (...) {
                lifetime::request_acquire_abandon(*presentation);
                if (call.alive() && state == State::WAIT_SUBMIT)
                    fail(Completion::Failure::SUBMIT_FAILED);
                sync::publish_terminal(*cancelled, Completion::Status::CANCELLED);
                throw;
            }
            sync::publish_terminal(*cancelled, Completion::Status::CANCELLED);
            return;
        }
        if (state == State::FINISH_SUBMIT)
            present();
        else if (state == State::ERROR_)
            lifetime::request_acquire_abandon(*presentation);
    }
    DefaultPhase::~DefaultPhase() {
        lifetime::PhaseCall::Invalidate(*this);
        const auto presentation = _presentation;
        if (presentation && (presentation->state.native_call_active || (submission && submission->result->native.active))) {
            lifetime::request_acquire_abandon(*presentation);
            if (submission)
                submission->abandon_requested = true;
            state = State::ERROR_;
        }
        finish_noexcept();
        if (presentation)
            lifetime::request_acquire_abandon(*presentation);
        if (presentation)
            lifetime::collect_acquire_scope(*presentation, presentation->state.status != sync::AcquisitionStatus::DELEGATED);
    }
} // namespace VSL_NAMESPACE

VSL_NAMESPACE::DefaultPhaseStream VSL_NAMESPACE::ComputePhase::operator<<(std::shared_ptr<command::Command> cmd) {
    DefaultPhaseStream op{this->manager};
    op << cmd;
    return op;
}

VSL_NAMESPACE::DefaultPhaseStream VSL_NAMESPACE::ComputePhase::operator<<(std::shared_ptr<command::DelegateCommand> cmd) {
    DefaultPhaseStream op{this->manager};
    op << cmd;
    return op;
}

VSL_NAMESPACE::DefaultPhaseStream& VSL_NAMESPACE::DefaultPhaseStream::operator<<(std::shared_ptr<command::Command> cmd) {
    auto current = manager;
    const auto buffer = current.get_buffer();
    const auto expected = buffer.completion();
    buffer.require_recording(expected);
    resolve_holder_require_ptr(cmd);
    buffer.require_recording(expected);
    cmd->invoke(current.get_pool(), buffer, current);
    return *this;
}

VSL_NAMESPACE::DefaultPhaseStream& VSL_NAMESPACE::DefaultPhaseStream::operator<<(std::shared_ptr<command::DelegateCommand> cmd) {
    auto current = manager;
    const auto buffer = current.get_buffer();
    const auto expected = buffer.completion();
    buffer.require_recording(expected);
    resolve_holder_require_ptr(cmd);
    buffer.require_recording(expected);
    cmd->invoke(*this, current.get_pool(), buffer, current);
    return *this;
}
