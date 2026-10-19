#include <VSL/vulkan/_pimpls.hpp>
#include <VSL/vulkan/phase.hpp>
#include <exception>
#include <stdexcept>

namespace VSL_NAMESPACE {
    Completion::Status Completion::status() const {
        if (!_data)
            return Status::Cancelled;
        if (_data->status == Status::Submitted &&
            _data->device->device.getFenceStatus(_data->fence) == vk::Result::eSuccess)
            _data->status = Status::Complete;
        return _data->status;
    }
    bool Completion::ready() const { return status() == Status::Complete; }
    bool Completion::wait(std::uint64_t timeout) const {
        auto current = status();
        if (current == Status::Complete)
            return true;
        if (current != Status::Submitted)
            return false;
        if (_data->device->device.waitForFences(_data->fence, true, timeout) != vk::Result::eSuccess)
            return false;
        _data->status = Status::Complete;
        return true;
    }
    void Completion::retain(std::shared_ptr<void> resource) const {
        if (!_data || _data->status != Status::Pending)
            throw std::logic_error("Resources must be retained before submission");
        _data->resources.push_back(std::move(resource));
    }
    _impl::Submission_impl::~Submission_impl() {
        if (status == Completion::Status::Submitted)
            (void)device->device.waitForFences(1, &fence, true, UINT64_MAX);
    }

    ComputePhase::ComputePhase(CommandManager manager,
                               std::optional<SemaphoreHolder> available,
                               std::optional<SemaphoreHolder> finished,
                               std::optional<FenceHolder> fence, bool postpone,
                               PipelineStage stage)
        : manager(manager), nextImageAvailable(available), calculationFinish(finished),
          inFlightFence(fence), waitStage(stage), initialExceptions(std::uncaught_exceptions()) {
        if (!postpone)
            setup();
    }

    void ComputePhase::prepare() {
        auto buffers = manager._data->commandBuffer;
        if (buffers->recording)
            throw std::logic_error("A phase is already recording on this manager");
        if (nextImageAvailable && (!nextImageAvailable->_data ||
                                   nextImageAvailable->_data->device != manager._data->device))
            throw std::invalid_argument("Acquire semaphore device mismatch");
        if (calculationFinish && (!calculationFinish->_data ||
                                  calculationFinish->_data->device != manager._data->device))
            throw std::invalid_argument("Signal semaphore device mismatch");
        if (inFlightFence && (!inFlightFence->_data ||
                              inFlightFence->_data->device != manager._data->device))
            throw std::invalid_argument("Completion fence device mismatch");
        frameSlot = buffers->currentBufferIdx;
        signalIndex = frameSlot;
        auto device = manager._data->device->device;
        if (buffers->submissions.size() < buffers->commandBuffers.size())
            buffers->submissions.resize(buffers->commandBuffers.size());
        auto previous = buffers->submissions.at(frameSlot);
        if (previous && previous->status == Completion::Status::Submitted)
            Completion{previous}.wait();
        if (nextImageAvailable && nextImageAvailable->_data->semaphores.size() <= frameSlot)
            throw std::invalid_argument("Acquire semaphore count must cover frame slots");
        if (inFlightFence && inFlightFence->_data->fences.size() <= frameSlot)
            throw std::invalid_argument("Fence count must cover frame slots");
        auto next = std::make_shared<_impl::Submission_impl>();
        next->device = manager._data->device;
        if (nextImageAvailable)
            next->resources.push_back(nextImageAvailable->_data);
        if (calculationFinish)
            next->resources.push_back(calculationFinish->_data);
        if (inFlightFence) {
            next->externalFence = inFlightFence->_data;
            next->externalFence->submissions.resize(next->externalFence->fences.size());
            next->fence = inFlightFence->_data->fences.at(frameSlot);
            if (!inFlightFence->wait(frameSlot, UINT64_MAX))
                throw std::runtime_error("Frame completion wait failed");
        } else {
            next->ownedFence = previous ? previous->ownedFence : nullptr;
            if (!next->ownedFence)
                next->ownedFence = std::make_shared<vk::UniqueFence>(device.createFenceUnique({}));
            next->fence = **next->ownedFence;
        }
        _completion = Completion{next};
        buffers->acquiredImageIndex.reset();
        buffers->acquiredSwapchain.reset();
    }

    void ComputePhase::begin() {
        auto buffers = manager._data->commandBuffer;
        buffers->commandBuffers.at(frameSlot).reset();
        buffers->commandBuffers.at(frameSlot).begin(vk::CommandBufferBeginInfo{});
        buffers->recording = true;
        buffers->recordingSubmission = _completion._data;
        state = State::WaitSubmit;
    }
    void ComputePhase::setup() {
        if (state != State::BeforeSetup)
            return;
        prepare();
        begin();
    }
    ComputePhase::SubmitResult ComputePhase::submit() {
        if (state != State::WaitSubmit)
            return SubmitResult::Failed;
        auto buffers = manager._data->commandBuffer;
        if (buffers->currentBufferIdx != frameSlot)
            throw std::logic_error("Frame slot changed while recording");
        std::vector<vk::Semaphore> waits, signals;
        if (nextImageAvailable)
            waits.push_back(nextImageAvailable->_data->semaphores.at(frameSlot));
        if (calculationFinish)
            signals.push_back(calculationFinish->_data->semaphores.at(signalIndex));
        else if (auto chain = buffers->acquiredSwapchain.lock())
            signals.push_back(*chain->presentationSemaphores.at(signalIndex));
        auto stages = vk::PipelineStageFlags(static_cast<std::uint32_t>(waitStage));
        if (!stages && !waits.empty())
            throw std::invalid_argument("Semaphore wait stage cannot be empty");
        auto command = buffers->commandBuffers.at(frameSlot);
        command.end();
        auto device = manager._data->device->device;
        device.resetFences(_completion._data->fence); // 画像の取得に成功するまでフェンスをリセットしない。
        vk::SubmitInfo info{
            .waitSemaphoreCount = static_cast<std::uint32_t>(waits.size()), .pWaitSemaphores = waits.data(), .pWaitDstStageMask = &stages, .commandBufferCount = 1, .pCommandBuffers = &command, .signalSemaphoreCount = static_cast<std::uint32_t>(signals.size()), .pSignalSemaphores = signals.data()};
        try {
            manager._data->graphicsQueue.submit(info, _completion._data->fence);
        } catch (...) {
            buffers->recording = false;
            buffers->recordingSubmission.reset();
            _completion._data->status = Completion::Status::Cancelled;
            state = State::Error;
            throw;
        }
        _completion._data->status = Completion::Status::Submitted;
        if (auto& external = _completion._data->externalFence)
            external->submissions.at(frameSlot) = _completion._data;
        buffers->submissions.at(frameSlot) = _completion._data;
        buffers->recording = false;
        buffers->recordingSubmission.reset();
        state = State::FinishSubmit;
        return SubmitResult::Success;
    }
    void ComputePhase::cancel() {
        if (state != State::WaitSubmit && state != State::BeforeSetup)
            return;
        if (_completion)
            _completion._data->status = Completion::Status::Cancelled;
        auto buffers = manager._data->commandBuffer;
        if (buffers->recordingSubmission == _completion._data) {
            buffers->recording = false;
            buffers->recordingSubmission.reset();
        }
        state = State::Error;
    }
    void ComputePhase::finishNoexcept() noexcept {
        try {
            if (std::uncaught_exceptions() > initialExceptions)
                cancel();
            else
                submit();
        } catch (...) {
            ComputePhase::cancel();
            loggingln("Phase submission failed during destruction");
        }
    }
    ComputePhase::~ComputePhase() { finishNoexcept(); }
    std::uint32_t ComputePhase::getImageIndex() { return imageIndex; }

    struct DefaultPhase::Presentation {
        vk::UniqueFence acquireFence;
    };
    DefaultPhase::DefaultPhase(CommandManager manager, SwapchainAccessor chain,
                               std::optional<SemaphoreHolder> available,
                               std::optional<SemaphoreHolder> finished,
                               std::optional<FenceHolder> fence, bool postpone)
        : ComputePhase(manager, available, finished, fence, true), swapchain(chain) {
        if (!postpone)
            setup();
    }
    void DefaultPhase::setup() {
        if (state != State::BeforeSetup)
            return;
        if (!swapchain._data || !swapchain._data->swapChain ||
            swapchain._data->device != manager._data->device)
            throw std::invalid_argument("A swapchain is required");
        auto buffers = manager._data->commandBuffer;
        if (buffers->recording)
            throw std::logic_error("A phase is already recording");
        buffers->currentBufferIdx = (buffers->currentBufferIdx + 1) % buffers->commandBuffers.size();
        prepare();
        const auto count = swapchain._data->swapChainImages.size();
        if (calculationFinish && calculationFinish->_data->semaphores.size() < count)
            throw std::invalid_argument("Present semaphores must cover swapchain images");
        auto device = manager._data->device->device;
        if (!calculationFinish && swapchain._data->presentationSemaphores.empty()) {
            for (std::size_t i = 0; i < count; ++i)
                swapchain._data->presentationSemaphores.push_back(device.createSemaphoreUnique({}));
        }
        vk::Semaphore available;
        vk::Fence acquired;
        if (nextImageAvailable)
            available = nextImageAvailable->_data->semaphores.at(frameSlot);
        else {
            presentation = std::make_shared<Presentation>();
            presentation->acquireFence = device.createFenceUnique({});
            acquired = *presentation->acquireFence;
        }
        auto result = device.acquireNextImageKHR(swapchain._data->swapChain, UINT64_MAX,
                                                 available, acquired, &imageIndex);
        if (result == vk::Result::eErrorOutOfDateKHR) {
            recreate = true;
            ComputePhase::cancel();
            return;
        }
        if (result != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR)
            throw std::runtime_error("Swapchain acquire failed: " + vk::to_string(result));
        recreate = result == vk::Result::eSuboptimalKHR;
        if (acquired && device.waitForFences(acquired, true, UINT64_MAX) != vk::Result::eSuccess)
            throw std::runtime_error("Swapchain acquire wait failed");
        signalIndex = imageIndex;
        begin();
        buffers->acquiredImageIndex = imageIndex;
        buffers->acquiredSwapchain = swapchain._data;
        _completion.retain(swapchain._data);
    }
    std::uint32_t DefaultPhase::getCurrentIndex() { return manager.getCurrentBufferIdx(); }
    DefaultPhase::SubmitResult DefaultPhase::submitCommands() {
        if (state == State::Error && recreate)
            return SubmitResult::WantRecreateSwapchain;
        return ComputePhase::submit();
    }
    DefaultPhase::SubmitResult DefaultPhase::present() {
        if (state != State::FinishSubmit || presented)
            return SubmitResult::Failed;
        vk::Semaphore finished = calculationFinish ? calculationFinish->_data->semaphores.at(imageIndex)
                                                   : *swapchain._data->presentationSemaphores.at(imageIndex);
        auto chain = swapchain._data->swapChain;
        vk::PresentInfoKHR info{.waitSemaphoreCount = 1, .pWaitSemaphores = &finished, .swapchainCount = 1, .pSwapchains = &chain, .pImageIndices = &imageIndex};
        auto result = manager._data->presentQueue.presentKHR(&info);
        presented = true;
        if (recreate || result == vk::Result::eSuboptimalKHR || result == vk::Result::eErrorOutOfDateKHR)
            return SubmitResult::WantRecreateSwapchain;
        if (result != vk::Result::eSuccess)
            return SubmitResult::Failed;
        return SubmitResult::Success;
    }
    DefaultPhase::SubmitResult DefaultPhase::submit() {
        if (state == State::Error && recreate)
            return SubmitResult::WantRecreateSwapchain;
        if (state == State::WaitSubmit && submitCommands() != SubmitResult::Success)
            return SubmitResult::Failed;
        return present();
    }
    void DefaultPhase::cancel() {
        if (imageIndex == UINT32_MAX || presented) {
            ComputePhase::cancel();
            return;
        }
        if (state == State::WaitSubmit) {
            // 途中の記録を破棄し、取得時のセマフォを消費して画像を画面表示に戻す。
            // 破棄した記録の完了状態はキャンセルのまま維持する。
            ComputePhase::cancel();
            prepare();
            signalIndex = imageIndex;
            begin();
            auto buffers = manager._data->commandBuffer;
            buffers->acquiredImageIndex = imageIndex;
            buffers->acquiredSwapchain = swapchain._data;
            _completion.retain(swapchain._data);
            vk::ImageMemoryBarrier barrier{
                .oldLayout = vk::ImageLayout::eUndefined, .newLayout = vk::ImageLayout::ePresentSrcKHR, .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED, .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED, .image = swapchain._data->swapChainImages.at(imageIndex), .subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}};
            buffers->commandBuffers.at(frameSlot).pipelineBarrier(
                vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eBottomOfPipe,
                {}, {}, {}, barrier);
            DefaultPhase::submitCommands(); // キャンセル時は派生クラスの終了コールバックを呼ばない。
        }
        if (state == State::FinishSubmit)
            present();
    }
    DefaultPhase::~DefaultPhase() { finishNoexcept(); }
} // namespace VSL_NAMESPACE

VSL_NAMESPACE::DefaultPhaseStream VSL_NAMESPACE::ComputePhase::operator<<(std::shared_ptr<command::__Command> cmd) {
    DefaultPhaseStream op{this->manager};
    op << cmd;
    return op;
}

VSL_NAMESPACE::DefaultPhaseStream VSL_NAMESPACE::ComputePhase::operator<<(std::shared_ptr<command::__DelegateCommand> cmd) {
    DefaultPhaseStream op{this->manager};
    op << cmd;
    return op;
}

VSL_NAMESPACE::DefaultPhaseStream& VSL_NAMESPACE::DefaultPhaseStream::operator<<(std::shared_ptr<command::__Command> cmd) {
    resolveHolderRequirePtr(cmd);
    cmd->invoke(manager.getPool(), manager.getBuffer(), manager);
    return *this;
}

VSL_NAMESPACE::DefaultPhaseStream& VSL_NAMESPACE::DefaultPhaseStream::operator<<(std::shared_ptr<command::__DelegateCommand> cmd) {
    resolveHolderRequirePtr(cmd);
    cmd->invoke(*this, manager.getPool(), manager.getBuffer(), manager);
    return *this;
}
