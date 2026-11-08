#include <VSL/vulkan/_pimpls.hpp>
#include <VSL/vulkan/phase.hpp>
#include <VSL/sync/submission_completion.hpp>
#include <VSL/lifetime/submission_retention.hpp>
#include <VSL/lifetime/swapchain_storage.hpp>
#include <VSL/lifetime/presentation_retention.hpp>
#include <VSL/lifetime/phase_call.hpp>
#include <VSL/lifetime/command_domain.hpp>
#include <VSL/lifetime/synchronization_use.hpp>
#include <VSL/sync/synchronization_state.hpp>
#include <VSL/sync/resource_use_access.hpp>
#include <VSL/operation/submission_ops.hpp>
#include <VSL/operation/command_ops.hpp>
#include <exception>
#include <stdexcept>
#include <algorithm>

namespace VSL_NAMESPACE {
    namespace {
        struct ReturningImage {
            lifetime::CommandRecordingContext& context;
            explicit ReturningImage(lifetime::CommandRecordingContext& context);
            ~ReturningImage() { context.returning_image = false; }
        };
        template<typename T>
        std::shared_ptr<T> synchronization_owner(const impl::SubmissionWork& work, impl::SubmissionSource source, T* value);
        size_t retain_synchronization_owner(impl::SubmissionWork& work, const std::shared_ptr<void>& owner);

        template<typename T>
        std::shared_ptr<T> synchronization_owner(const impl::SubmissionWork& work, impl::SubmissionSource source, T* value) {
            if (!value)
                return {};
            return {work.resources.at(work.synchronization_owners.at(static_cast<size_t>(source))), value};
        }
        size_t retain_synchronization_owner(impl::SubmissionWork& work, const std::shared_ptr<void>& owner) {
            if (!owner.use_count())
                return impl::SubmissionWork::NO_OWNER;
            lifetime::retain_resource(work.resources, owner);
            const auto found = std::ranges::find_if(work.resources, [&](const auto& existing) {
                return !existing.owner_before(owner) && !owner.owner_before(existing);
            });
            return static_cast<size_t>(found - work.resources.begin());
        }
        ReturningImage::ReturningImage(lifetime::CommandRecordingContext& context) : context(context) {
            if (context.returning_image)
                throw std::logic_error("Acquired image is already being returned");
            context.returning_image = true;
        }
    } // namespace
    struct DefaultPhase::Presentation {
        std::shared_ptr<impl::LogicalDeviceImpl> device;
        std::shared_ptr<impl::SemaphoreImpl> available;
        std::shared_ptr<impl::SemaphoreImpl> finished;
        std::shared_ptr<sync::SemaphoreWSIUse> available_use, finished_use;
        std::shared_ptr<impl::SwapchainImpl> swapchain;
        std::shared_ptr<lifetime::SurfacePresentationDomain> presentations;
        std::shared_ptr<lifetime::AcquireEvidence> evidence;
        std::shared_ptr<lifetime::PresentRecord> record;
        vk::UniqueFence acquire_fence;
        std::uint32_t acquired_index = UINT32_MAX;
        bool acquired = false;
        void reject_acquisition() noexcept;
        ~Presentation();
    };

    void DefaultPhase::Presentation::reject_acquisition() noexcept {
        if (evidence) {
            evidence->proof.status = sync::FenceProofStatus::NOT_ACQUIRED;
            evidence->finished = true;
        }
        if (available_use)
            available_use->complete = true;
        if (finished_use)
            finished_use->complete = true;
    }
    DefaultPhase::Presentation::~Presentation() {
        if (acquired && evidence)
            while (!sync::fence_proof_terminal(evidence->proof))
                sync::wait_fence_proof(evidence->proof);
        if (!acquired)
            reject_acquisition();
        if (available_use)
            available_use->complete = true;
        if (finished_use)
            finished_use->complete = true;
    }

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
        if (domain->context.closing || domain->context.recording || domain->context.mutating ||
            (!returning_image && domain->context.frame_buffer_recording))
            throw std::logic_error("Command domain is unavailable for recording");
        const auto generation = domain->context.generation;
        const auto manager_data = manager.data_;
        const auto inherited = returning_image ? submission : nullptr;
        const auto available = inherited ? synchronization_owner(*inherited, impl::SubmissionSource::WAIT_SEMAPHORE, inherited->wait_semaphore)
                                         : (next_image_available ? next_image_available->data_ : nullptr);
        const auto finished = inherited ? synchronization_owner(*inherited, impl::SubmissionSource::SIGNAL_SEMAPHORE, inherited->signal_semaphore)
                                        : (calculation_finish ? calculation_finish->data_ : nullptr);
        const auto external = inherited ? synchronization_owner(*inherited, impl::SubmissionSource::FENCE, inherited->external_fence)
                                        : (in_flight_fence ? in_flight_fence->data_ : nullptr);
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
        if (!inherited && previous && previous->result->status == Completion::Status::SUBMITTED) {
            const auto completed = Completion{previous->result}.wait();
            if (!call.alive())
                return false;
            if (!completed)
                throw std::runtime_error("Previous frame completion failed");
        }
        if (available && available->semaphores.size() <= frame_slot)
            throw std::invalid_argument("Acquire semaphore count must cover frame slots");
        if (external && external->fences.size() <= frame_slot)
            throw std::invalid_argument("Fence count must cover frame slots");
        auto next = std::make_shared<impl::SubmissionWork>();
        next->device = manager_data->device;
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
        if (domain->context.closing || domain->context.recording || domain->context.generation != generation ||
            manager.data_ != manager_data || manager_data->command_buffer != buffers ||
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
        const auto buffers = buffer.data_;
        auto* context = buffers ? buffers->recording_context : nullptr;
        if (work && context && context->work == work.get()) {
            try {
                operation::reset_command_buffer(buffers->command_buffers.at(frame_slot));
            } catch (...) {
            }
            context->recording = false;
            context->work = nullptr;
            lifetime::CommandAccess::Owner(buffer)->recording_submission.reset();
        }
        state = State::ERROR_;
        if (work)
            sync::publish_terminal(*work, Completion::Status::FAILED, failure, error_code);
    }
    void ComputePhase::begin() {
        const auto domain = lifetime::CommandAccess::Owner(recording_buffer);
        auto buffers = domain->buffer;
        try {
            if (domain->context.closing || domain->context.recording || manager.data_->command_buffer != buffers ||
                buffers->current_buffer_idx != frame_slot ||
                (domain->submissions.at(frame_slot) && domain->submissions.at(frame_slot)->result->status == Completion::Status::SUBMITTED))
                throw std::logic_error("Frame slot is no longer available for recording");
            operation::reset_command_buffer(buffers->command_buffers.at(frame_slot));
            operation::begin_command_buffer(buffers->command_buffers.at(frame_slot));
        } catch (const vk::SystemError& error) {
            const auto failure = error.code().value() == static_cast<int>(vk::Result::eErrorDeviceLost) ? Completion::Failure::DEVICE_LOST : Completion::Failure::SUBMIT_FAILED;
            fail(failure, error.code().value());
            throw;
        } catch (...) {
            fail(Completion::Failure::SUBMIT_FAILED);
            throw;
        }
        domain->recording_submission = submission;
        domain->context.work = submission.get();
        domain->context.recording = true;
        ++domain->context.generation;
        state = State::WAIT_SUBMIT;
    }
    void ComputePhase::setup() {
        lifetime::PhaseCall call(*this);
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
        if (state != State::WAIT_SUBMIT)
            return SubmitResult::FAILED;
        const auto domain = lifetime::CommandAccess::Owner(recording_buffer);
        auto buffers = domain->buffer;
        try {
            if (domain->context.closing || !domain->context.recording || domain->context.work != submission.get() ||
                manager.data_->command_buffer != buffers || buffers->current_buffer_idx != frame_slot)
                throw std::logic_error("Frame slot changed while recording");
            lifetime::require_fence_reservation(*submission, frame_slot);
            if (!domain->context.returning_image &&
                ((next_image_available ? next_image_available->data_.get() : nullptr) != submission->wait_semaphore ||
                 (calculation_finish ? calculation_finish->data_.get() : nullptr) != submission->signal_semaphore ||
                 (in_flight_fence ? in_flight_fence->data_.get() : nullptr) != submission->external_fence))
                throw std::logic_error("Phase synchronization owners changed after preparation");
            std::vector<vk::Semaphore> waits, signals;
            if (submission->wait_semaphore)
                waits.push_back(submission->wait_semaphore->semaphores.at(frame_slot));
            if (submission->signal_semaphore)
                signals.push_back(submission->signal_semaphore->semaphores.at(signal_index));
            else if (auto chain = domain->context.acquired_swapchain.lock())
                signals.push_back(*chain->presentation_semaphores.at(signal_index));
            auto stages = vk::PipelineStageFlags(static_cast<std::uint32_t>(wait_stage));
            if (!stages && !waits.empty())
                throw std::invalid_argument("Semaphore wait stage cannot be empty");
            auto command = buffers->command_buffers.at(frame_slot);
            operation::end_command_buffer(command);
            auto device = manager.data_->device->device;
            device.resetFences(submission->fence);
            vk::SubmitInfo info{
                .waitSemaphoreCount = static_cast<std::uint32_t>(waits.size()), .pWaitSemaphores = waits.data(), .pWaitDstStageMask = &stages, .commandBufferCount = 1, .pCommandBuffers = &command, .signalSemaphoreCount = static_cast<std::uint32_t>(signals.size()), .pSignalSemaphores = signals.data()};
            manager.data_->graphics_queue.submit(info, submission->fence);
        } catch (const vk::SystemError& error) {
            const auto failure = error.code().value() == static_cast<int>(vk::Result::eErrorDeviceLost) ? Completion::Failure::DEVICE_LOST : Completion::Failure::SUBMIT_FAILED;
            fail(failure, error.code().value());
            throw;
        } catch (...) {
            fail(Completion::Failure::SUBMIT_FAILED);
            throw;
        }
        completion_.data_->status = Completion::Status::SUBMITTED;
        if (auto& external = submission->external_fence)
            external->uses.at(frame_slot).submitted = completion_.data_;
        auto previous = std::move(domain->submissions.at(frame_slot));
        domain->submissions.at(frame_slot) = submission;
        domain->context.recording = false;
        domain->context.work = nullptr;
        domain->recording_submission.reset();
        state = State::FINISH_SUBMIT;
        return SubmitResult::SUCCESS;
    }
    Completion::Failure ComputePhase::cancel_recording(std::int32_t& error_code) {
        const auto buffers = recording_buffer.data_;
        auto* context = buffers ? buffers->recording_context : nullptr;
        Completion::Failure failure = Completion::Failure::NONE;
        if (submission && context && context->work == submission.get()) {
            try {
                operation::reset_command_buffer(buffers->command_buffers.at(frame_slot));
            } catch (const vk::SystemError& error) {
                error_code = error.code().value();
                failure = error_code == static_cast<int>(vk::Result::eErrorDeviceLost) ? Completion::Failure::DEVICE_LOST : Completion::Failure::SUBMIT_FAILED;
            } catch (...) {
                failure = Completion::Failure::SUBMIT_FAILED;
            }
            context->recording = false;
            context->work = nullptr;
            lifetime::CommandAccess::Owner(recording_buffer)->recording_submission.reset();
        }
        state = State::ERROR_;
        return failure;
    }
    void ComputePhase::cancel() {
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
                               std::optional<FenceHolder> fence, bool postpone)
        : ComputePhase(manager, available, finished, fence, true), swapchain(chain) {
        if (!postpone)
            setup();
    }
    void DefaultPhase::setup() {
        lifetime::PhaseCall call(*this);
        if (state != State::BEFORE_SETUP)
            return;
        try {
            const auto chain = swapchain.data_;
            if (!chain || !chain->swap_chain ||
                chain->device != manager.data_->device.get())
                throw std::invalid_argument("A swapchain is required");
            lifetime::require_swapchain_owner(chain);
            operation::require_active_swapchain(*chain);
            const auto buffer = manager.get_buffer();
            const auto domain = lifetime::CommandAccess::Owner(buffer);
            auto buffers = domain->buffer;
            if (domain->context.recording || domain->context.returning_image || domain->context.mutating || domain->context.frame_buffer_recording)
                throw std::logic_error("A phase is already recording");
            buffers->current_buffer_idx = (buffers->current_buffer_idx + 1) % buffers->command_buffers.size();
            if (!prepare() || !call.alive())
                return;
            if (swapchain.data_ != chain)
                throw std::logic_error("Swapchain changed while preparing a phase");
            operation::require_active_swapchain(*chain);
            const auto count = chain->swap_chain_images.size();
            if (submission->signal_semaphore && submission->signal_semaphore->semaphores.size() < count)
                throw std::invalid_argument("Present semaphores must cover swapchain images");
            auto device = manager.data_->device->device;
            if (!submission->signal_semaphore && chain->presentation_semaphores.empty()) {
                std::vector<vk::UniqueSemaphore> semaphores;
                semaphores.reserve(count);
                for (std::size_t i = 0; i < count; ++i)
                    semaphores.push_back(device.createSemaphoreUnique({}));
                chain->presentation_semaphores = std::move(semaphores);
            }
            vk::Semaphore available;
            _presentation = std::make_shared<Presentation>();
            _presentation->device = manager.data_->device;
            _presentation->swapchain = chain;
            _presentation->presentations = lifetime::presentation_domain(chain);
            lifetime::register_presentation_domain(*domain, _presentation->presentations);
            _presentation->acquire_fence = device.createFenceUnique({});
            _presentation->evidence = lifetime::prepare_acquire_evidence(*_presentation->presentations, chain, *_presentation->acquire_fence);
            if (submission->wait_semaphore) {
                _presentation->available = synchronization_owner(*submission, impl::SubmissionSource::WAIT_SEMAPHORE, submission->wait_semaphore);
                _presentation->available_use = sync::track_semaphore_wsi(*_presentation->available, submission->result);
                available = _presentation->available->semaphores.at(frame_slot);
            }
            if (submission->signal_semaphore) {
                _presentation->finished = synchronization_owner(*submission, impl::SubmissionSource::SIGNAL_SEMAPHORE, submission->signal_semaphore);
                _presentation->finished_use = sync::track_semaphore_wsi(*_presentation->finished);
            }
            const auto acquired = *_presentation->acquire_fence;
            lifetime::retain_until_complete(*submission, _presentation);
            if (swapchain.data_ != chain || !chain->swap_chain || chain->state != SwapchainState::ACTIVE) {
                _presentation->reject_acquisition();
                throw std::logic_error("Swapchain changed before image acquisition");
            }
            std::uint32_t acquired_index = UINT32_MAX;
            auto result = device.acquireNextImageKHR(chain->swap_chain, UINT64_MAX,
                                                     available, acquired, &acquired_index);
            if (result == vk::Result::eErrorOutOfDateKHR) {
                _presentation->reject_acquisition();
                recreate = true;
                ComputePhase::cancel();
                return;
            }
            if (result != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR) {
                _presentation->reject_acquisition();
                fail(result == vk::Result::eErrorDeviceLost ? Completion::Failure::DEVICE_LOST : Completion::Failure::SUBMIT_FAILED, static_cast<std::int32_t>(result));
                throw std::runtime_error("Swapchain acquire failed: " + vk::to_string(result));
            }
            _presentation->acquired = true;
            _presentation->acquired_index = acquired_index;
            lifetime::commit_acquire_evidence(_presentation->evidence, *chain, acquired_index);
            image_index = acquired_index;
            recreate = result == vk::Result::eSuboptimalKHR;
            if (!available && !sync::wait_fence_proof(_presentation->evidence->proof))
                throw std::runtime_error("Swapchain acquire wait failed");
            signal_index = _presentation->acquired_index;
            begin();
            domain->context.acquired_image_index = _presentation->acquired_index;
            domain->context.acquired_swapchain = chain;
        } catch (const vk::SystemError& error) {
            if (!call.alive())
                throw;
            if (_presentation && !_presentation->acquired)
                _presentation->reject_acquisition();
            fail(error.code().value() == static_cast<int>(vk::Result::eErrorDeviceLost) ? Completion::Failure::DEVICE_LOST : Completion::Failure::SUBMIT_FAILED, error.code().value());
            throw;
        } catch (...) {
            if (!call.alive())
                throw;
            if (_presentation && !_presentation->acquired)
                _presentation->reject_acquisition();
            fail(Completion::Failure::SUBMIT_FAILED);
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
        if (state != State::FINISH_SUBMIT || _presented || !_presentation || !_presentation->acquired)
            return SubmitResult::FAILED;
        const auto presentation = _presentation;
        const auto domain = lifetime::CommandAccess::Owner(recording_buffer);
        const auto registry = presentation->presentations;
        if (!presentation->record) {
            lifetime::register_presentation_domain(*domain, registry);
            presentation->record = lifetime::prepare_present_record(*registry, presentation->swapchain, presentation->finished,
                                                                    presentation->finished_use, manager.data_->present_queue, presentation->acquired_index);
            presentation->finished_use.reset();
        }
        const auto outcome = lifetime::enqueue_present_record(*presentation->record);
        const auto native = presentation->record->state.result;
        _presented = outcome != sync::PresentEnqueue::NOT_ENQUEUED;
        auto result = SubmitResult::FAILED;
        if (outcome == sync::PresentEnqueue::ENQUEUED) {
            if (recreate || native == vk::Result::eSuboptimalKHR || native == vk::Result::eErrorOutOfDateKHR)
                result = SubmitResult::WANT_RECREATE_SWAPCHAIN;
            else if (native == vk::Result::eSuccess)
                result = SubmitResult::SUCCESS;
        }
        lifetime::collect_quarantined_presentations(registry->surface, presentation->swapchain->native_device);
        lifetime::collect_presentations(registry);
        return result; // callbackがPhaseを破棄しても、以降thisを参照しない。
    }
    DefaultPhase::SubmitResult DefaultPhase::submit() {
        if (state == State::ERROR_ && recreate)
            return SubmitResult::WANT_RECREATE_SWAPCHAIN;
        const auto domain = recording_buffer.data_ ? lifetime::CommandAccess::Owner(recording_buffer) : nullptr;
        const auto previous = domain ? domain->submissions.at(frame_slot) : nullptr;
        if (state == State::WAIT_SUBMIT && submit_commands() != SubmitResult::SUCCESS)
            return SubmitResult::FAILED;
        return present();
    }
    void DefaultPhase::cancel() {
        lifetime::PhaseCall call(*this);
        if (!_presentation || !_presentation->acquired || _presented) {
            ComputePhase::cancel();
            return;
        }
        if (state == State::WAIT_SUBMIT) {
            const auto index = _presentation->acquired_index;
            const auto generation = _presentation->swapchain;
            const auto domain = lifetime::CommandAccess::Owner(recording_buffer);
            const ReturningImage returning(domain->context);
            const auto cancelled = submission;
            const auto previous = domain->submissions.at(frame_slot);
            std::int32_t error_code = 0;
            const auto failure = cancel_recording(error_code);
            if (failure != Completion::Failure::NONE) {
                sync::publish_terminal(*cancelled, Completion::Status::FAILED, failure, error_code);
                return;
            }
            // 途中の記録を破棄し、取得時のセマフォを消費して画像を画面表示に戻す。
            // 元結果のcallbackは返却の記録・送信後に呼び、Phase自己破棄後にthisへ触れない。
            try {
                if (!prepare(true) || !call.alive())
                    return;
                if (_presentation->available_use)
                    _presentation->available_use->acquisition = submission->result;
                signal_index = index;
                begin();
                auto buffers = domain->buffer;
                domain->context.acquired_image_index = index;
                domain->context.acquired_swapchain = generation;
                lifetime::retain_until_complete(*submission, _presentation);
                const auto image = lifetime::swapchain_image(generation, index);
                sync::ResourceUseAccess::TrackPending(*image->use, completion_);
                vk::ImageMemoryBarrier barrier{
                    .oldLayout = vk::ImageLayout::eUndefined, .newLayout = vk::ImageLayout::ePresentSrcKHR, .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED, .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED, .image = generation->swap_chain_images.at(index), .subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}};
                buffers->command_buffers.at(frame_slot).pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eBottomOfPipe, {}, {}, {}, barrier);
                DefaultPhase::submit_commands(); // キャンセル時は派生クラスの終了コールバックを呼ばない。
                if (state == State::FINISH_SUBMIT)
                    present();
            } catch (...) {
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
    }
    DefaultPhase::~DefaultPhase() {
        lifetime::PhaseCall::Invalidate(*this);
        finish_noexcept();
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
    resolve_holder_require_ptr(cmd);
    cmd->invoke(manager.get_pool(), manager.get_buffer(), manager);
    return *this;
}

VSL_NAMESPACE::DefaultPhaseStream& VSL_NAMESPACE::DefaultPhaseStream::operator<<(std::shared_ptr<command::DelegateCommand> cmd) {
    resolve_holder_require_ptr(cmd);
    cmd->invoke(*this, manager.get_pool(), manager.get_buffer(), manager);
    return *this;
}
