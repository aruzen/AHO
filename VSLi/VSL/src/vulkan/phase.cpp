#include <VSL/vulkan/_pimpls.hpp>
#include <VSL/vulkan/phase.hpp>
#include <VSL/sync/submission_completion.hpp>
#include <VSL/lifetime/submission_retention.hpp>
#include <VSL/operation/submission_ops.hpp>
#include <exception>
#include <stdexcept>

namespace VSL_NAMESPACE {
    struct DefaultPhase::Presentation {
        std::shared_ptr<impl::LogicalDeviceImpl> device;
        std::shared_ptr<impl::SemaphoreImpl> available;
        std::shared_ptr<impl::SwapchainImpl> swapchain;
        vk::UniqueFence acquire_fence;
        bool acquired = false;
        ~Presentation();
    };

    DefaultPhase::Presentation::~Presentation() {
        if (!acquired)
            return;
        auto result = operation::wait_fence(device->device, *acquire_fence, UINT64_MAX);
        while (result != vk::Result::eSuccess && result != vk::Result::eErrorDeviceLost)
            result = operation::wait_fence(device->device, *acquire_fence, UINT64_MAX);
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

    void ComputePhase::prepare() {
        auto buffers = manager.data_->command_buffer;
        if (buffers->recording)
            throw std::logic_error("A phase is already recording on this manager");
        if (next_image_available && (!next_image_available->data_ ||
                                     next_image_available->data_->device != manager.data_->device))
            throw std::invalid_argument("Acquire semaphore device mismatch");
        if (calculation_finish && (!calculation_finish->data_ ||
                                   calculation_finish->data_->device != manager.data_->device))
            throw std::invalid_argument("Signal semaphore device mismatch");
        if (in_flight_fence && (!in_flight_fence->data_ ||
                                in_flight_fence->data_->device != manager.data_->device))
            throw std::invalid_argument("Completion fence device mismatch");
        frame_slot = buffers->current_buffer_idx;
        signal_index = frame_slot;
        auto device = manager.data_->device->device;
        if (buffers->submissions.size() < buffers->command_buffers.size())
            buffers->submissions.resize(buffers->command_buffers.size());
        auto previous = buffers->submissions.at(frame_slot);
        if (previous && previous->result->status == Completion::Status::SUBMITTED && !Completion{previous->result}.wait())
            throw std::runtime_error("Previous frame completion failed");
        if (next_image_available && next_image_available->data_->semaphores.size() <= frame_slot)
            throw std::invalid_argument("Acquire semaphore count must cover frame slots");
        if (in_flight_fence && in_flight_fence->data_->fences.size() <= frame_slot)
            throw std::invalid_argument("Fence count must cover frame slots");
        auto next = std::make_shared<impl::SubmissionWork>();
        next->device = manager.data_->device;
        if (next_image_available)
            next->resources.push_back(next_image_available->data_);
        if (calculation_finish)
            next->resources.push_back(calculation_finish->data_);
        if (in_flight_fence) {
            next->external_fence = in_flight_fence->data_;
            next->external_fence->submissions.resize(next->external_fence->fences.size());
            next->fence = in_flight_fence->data_->fences.at(frame_slot);
            if (!in_flight_fence->wait(frame_slot, UINT64_MAX))
                throw std::runtime_error("Frame completion wait failed");
        } else {
            next->owned_fence = previous ? previous->owned_fence : nullptr;
            if (!next->owned_fence)
                next->owned_fence = std::make_shared<vk::UniqueFence>(device.createFenceUnique({}));
            next->fence = **next->owned_fence;
        }
        if (buffers->recording || buffers->current_buffer_idx != frame_slot || buffers->submissions.at(frame_slot) != previous)
            throw std::logic_error("Frame slot changed while preparing");
        next->result->work = next;
        submission = std::move(next);
        completion_ = Completion{submission->result};
        buffers->acquired_image_index.reset();
        buffers->acquired_swapchain.reset();
    }

    void ComputePhase::fail(Completion::Failure failure, std::int32_t error_code) noexcept {
        auto buffers = manager.data_->command_buffer;
        if (submission && buffers->recording_submission == submission) {
            try {
                buffers->command_buffers.at(frame_slot).reset();
            } catch (...) {
            }
            buffers->recording = false;
            buffers->recording_submission.reset();
        }
        if (submission)
            sync::publish_terminal(*submission, Completion::Status::FAILED, failure, error_code);
        state = State::ERROR_;
    }
    void ComputePhase::begin() {
        auto buffers = manager.data_->command_buffer;
        try {
            if (buffers->recording || buffers->current_buffer_idx != frame_slot ||
                (buffers->submissions.at(frame_slot) && buffers->submissions.at(frame_slot)->result->status == Completion::Status::SUBMITTED))
                throw std::logic_error("Frame slot is no longer available for recording");
            buffers->command_buffers.at(frame_slot).reset();
            buffers->command_buffers.at(frame_slot).begin(vk::CommandBufferBeginInfo{});
        } catch (const vk::SystemError& error) {
            const auto failure = error.code().value() == static_cast<int>(vk::Result::eErrorDeviceLost) ? Completion::Failure::DEVICE_LOST : Completion::Failure::SUBMIT_FAILED;
            fail(failure, error.code().value());
            throw;
        } catch (...) {
            fail(Completion::Failure::SUBMIT_FAILED);
            throw;
        }
        buffers->recording = true;
        buffers->recording_submission = submission;
        state = State::WAIT_SUBMIT;
    }
    void ComputePhase::setup() {
        if (state != State::BEFORE_SETUP)
            return;
        try {
            prepare();
            begin();
        } catch (const vk::SystemError& error) {
            fail(error.code().value() == static_cast<int>(vk::Result::eErrorDeviceLost) ? Completion::Failure::DEVICE_LOST : Completion::Failure::SUBMIT_FAILED, error.code().value());
            throw;
        } catch (...) {
            fail(Completion::Failure::SUBMIT_FAILED);
            throw;
        }
    }
    ComputePhase::SubmitResult ComputePhase::submit() {
        if (state != State::WAIT_SUBMIT)
            return SubmitResult::FAILED;
        auto buffers = manager.data_->command_buffer;
        try {
            if (buffers->current_buffer_idx != frame_slot)
                throw std::logic_error("Frame slot changed while recording");
            std::vector<vk::Semaphore> waits, signals;
            if (next_image_available)
                waits.push_back(next_image_available->data_->semaphores.at(frame_slot));
            if (calculation_finish)
                signals.push_back(calculation_finish->data_->semaphores.at(signal_index));
            else if (auto chain = buffers->acquired_swapchain.lock())
                signals.push_back(*chain->presentation_semaphores.at(signal_index));
            auto stages = vk::PipelineStageFlags(static_cast<std::uint32_t>(wait_stage));
            if (!stages && !waits.empty())
                throw std::invalid_argument("Semaphore wait stage cannot be empty");
            auto command = buffers->command_buffers.at(frame_slot);
            command.end();
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
            external->submissions.at(frame_slot) = completion_.data_;
        buffers->submissions.at(frame_slot) = submission;
        buffers->recording = false;
        buffers->recording_submission.reset();
        state = State::FINISH_SUBMIT;
        return SubmitResult::SUCCESS;
    }
    void ComputePhase::cancel() {
        if (state != State::WAIT_SUBMIT && state != State::BEFORE_SETUP)
            return;
        auto buffers = manager.data_->command_buffer;
        Completion::Failure failure = Completion::Failure::NONE;
        std::int32_t error_code = 0;
        if (submission && buffers->recording_submission == submission) {
            try {
                buffers->command_buffers.at(frame_slot).reset();
            } catch (const vk::SystemError& error) {
                error_code = error.code().value();
                failure = error_code == static_cast<int>(vk::Result::eErrorDeviceLost) ? Completion::Failure::DEVICE_LOST : Completion::Failure::SUBMIT_FAILED;
            } catch (...) {
                failure = Completion::Failure::SUBMIT_FAILED;
            }
            buffers->recording = false;
            buffers->recording_submission.reset();
        }
        if (submission)
            sync::publish_terminal(*submission, failure == Completion::Failure::NONE ? Completion::Status::CANCELLED : Completion::Status::FAILED, failure, error_code);
        state = State::ERROR_;
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
        if (state != State::BEFORE_SETUP)
            return;
        try {
            if (!swapchain.data_ || !swapchain.data_->swap_chain ||
                swapchain.data_->device != manager.data_->device)
                throw std::invalid_argument("A swapchain is required");
            auto buffers = manager.data_->command_buffer;
            if (buffers->recording)
                throw std::logic_error("A phase is already recording");
            buffers->current_buffer_idx = (buffers->current_buffer_idx + 1) % buffers->command_buffers.size();
            prepare();
            const auto count = swapchain.data_->swap_chain_images.size();
            if (calculation_finish && calculation_finish->data_->semaphores.size() < count)
                throw std::invalid_argument("Present semaphores must cover swapchain images");
            auto device = manager.data_->device->device;
            if (!calculation_finish && swapchain.data_->presentation_semaphores.empty()) {
                for (std::size_t i = 0; i < count; ++i)
                    swapchain.data_->presentation_semaphores.push_back(device.createSemaphoreUnique({}));
            }
            vk::Semaphore available;
            _presentation = std::make_shared<Presentation>();
            _presentation->device = manager.data_->device;
            _presentation->swapchain = swapchain.data_;
            _presentation->acquire_fence = device.createFenceUnique({});
            if (next_image_available) {
                _presentation->available = next_image_available->data_;
                available = next_image_available->data_->semaphores.at(frame_slot);
            }
            const auto acquired = *_presentation->acquire_fence;
            completion_.retain(_presentation);
            lifetime::retain_resource(buffers->presentation_resources, swapchain.data_);
            auto result = device.acquireNextImageKHR(swapchain.data_->swap_chain, UINT64_MAX,
                                                     available, acquired, &image_index);
            if (result == vk::Result::eErrorOutOfDateKHR) {
                recreate = true;
                ComputePhase::cancel();
                return;
            }
            if (result != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR) {
                fail(result == vk::Result::eErrorDeviceLost ? Completion::Failure::DEVICE_LOST : Completion::Failure::SUBMIT_FAILED, static_cast<std::int32_t>(result));
                throw std::runtime_error("Swapchain acquire failed: " + vk::to_string(result));
            }
            _presentation->acquired = true;
            recreate = result == vk::Result::eSuboptimalKHR;
            if (!available && device.waitForFences(acquired, true, UINT64_MAX) != vk::Result::eSuccess)
                throw std::runtime_error("Swapchain acquire wait failed");
            signal_index = image_index;
            begin();
            buffers->acquired_image_index = image_index;
            buffers->acquired_swapchain = swapchain.data_;
            completion_.retain(swapchain.data_);
            lifetime::retain_resource(buffers->presentation_resources, swapchain.data_);
            if (calculation_finish)
                lifetime::retain_resource(buffers->presentation_resources, calculation_finish->data_);
        } catch (const vk::SystemError& error) {
            fail(error.code().value() == static_cast<int>(vk::Result::eErrorDeviceLost) ? Completion::Failure::DEVICE_LOST : Completion::Failure::SUBMIT_FAILED, error.code().value());
            throw;
        } catch (...) {
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
        if (state != State::FINISH_SUBMIT || _presented)
            return SubmitResult::FAILED;
        vk::Semaphore finished = calculation_finish ? calculation_finish->data_->semaphores.at(image_index)
                                                    : *swapchain.data_->presentation_semaphores.at(image_index);
        auto chain = swapchain.data_->swap_chain;
        vk::PresentInfoKHR info{.waitSemaphoreCount = 1, .pWaitSemaphores = &finished, .swapchainCount = 1, .pSwapchains = &chain, .pImageIndices = &image_index};
        lifetime::retain_resource(manager.data_->command_buffer->presentation_resources, swapchain.data_);
        if (calculation_finish)
            lifetime::retain_resource(manager.data_->command_buffer->presentation_resources, calculation_finish->data_);
        auto result = manager.data_->present_queue.presentKHR(&info);
        _presented = true;
        if (recreate || result == vk::Result::eSuboptimalKHR || result == vk::Result::eErrorOutOfDateKHR)
            return SubmitResult::WANT_RECREATE_SWAPCHAIN;
        if (result != vk::Result::eSuccess)
            return SubmitResult::FAILED;
        return SubmitResult::SUCCESS;
    }
    DefaultPhase::SubmitResult DefaultPhase::submit() {
        if (state == State::ERROR_ && recreate)
            return SubmitResult::WANT_RECREATE_SWAPCHAIN;
        if (state == State::WAIT_SUBMIT && submit_commands() != SubmitResult::SUCCESS)
            return SubmitResult::FAILED;
        return present();
    }
    void DefaultPhase::cancel() {
        if (image_index == UINT32_MAX || _presented) {
            ComputePhase::cancel();
            return;
        }
        if (state == State::WAIT_SUBMIT) {
            // 途中の記録を破棄し、取得時のセマフォを消費して画像を画面表示に戻す。
            // 破棄した記録の完了状態はキャンセルのまま維持する。
            ComputePhase::cancel();
            prepare();
            signal_index = image_index;
            begin();
            auto buffers = manager.data_->command_buffer;
            buffers->acquired_image_index = image_index;
            buffers->acquired_swapchain = swapchain.data_;
            completion_.retain(swapchain.data_);
            vk::ImageMemoryBarrier barrier{
                .oldLayout = vk::ImageLayout::eUndefined, .newLayout = vk::ImageLayout::ePresentSrcKHR, .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED, .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED, .image = swapchain.data_->swap_chain_images.at(image_index), .subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}};
            buffers->command_buffers.at(frame_slot).pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eBottomOfPipe, {}, {}, {}, barrier);
            DefaultPhase::submit_commands(); // キャンセル時は派生クラスの終了コールバックを呼ばない。
        }
        if (state == State::FINISH_SUBMIT)
            present();
    }
    DefaultPhase::~DefaultPhase() {
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
