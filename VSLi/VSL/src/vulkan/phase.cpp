//
// Created by morimoto_hibiki on 2025/07/30.
//
#include <VSL/define.hpp>

#include <VSL/vulkan/_pimpls.hpp>
#include <VSL/exceptions.hpp>

#include <VSL/vulkan/phase.hpp>

VSL_NAMESPACE::DefaultPhase::DefaultPhase(CommandManager _manager, SwapchainAccessor swapchain,
                                          std::optional<SemaphoreHolder> nextImageAvailable,
                                          std::optional<SemaphoreHolder> calculationFinish,
                                          std::optional<FenceHolder> inFlightFence,
                                          bool postponeSetup)
        : ComputePhase(_manager, nextImageAvailable, calculationFinish, inFlightFence, true)
        , swapchain(swapchain) {
    if (not postponeSetup)
        setup();
}

void vsl::DefaultPhase::setup() {
    if (state != State::BeforeSetup)
        return;

    manager._data->commandBuffer->currentBufferIdx = (manager._data->commandBuffer->currentBufferIdx + 1) % manager._data->commandBuffer->commandBuffers.size();
    // FIXME: Result check
    if (nextImageAvailable.has_value())
        imageIndex = manager._data->device->device.acquireNextImageKHR(swapchain._data->swapChain, UINT64_MAX
                , nextImageAvailable.value()._data->semaphores[getCurrentIndex()],nullptr).value;
    else
        imageIndex = manager._data->device->device.acquireNextImageKHR(swapchain._data->swapChain, UINT64_MAX, nullptr, nullptr).value;

    ComputePhase::setup();
}

vsl::DefaultPhase::SubmitResult vsl::DefaultPhase::submit() {
    if (state != State::WaitSubmit)
        return vsl::DefaultPhase::SubmitResult::Failed;
    auto r = ComputePhase::submit();
    if (r != vsl::DefaultPhase::SubmitResult::Success)
        return r;

    std::vector<vk::Semaphore> signalSemaphores;
    if (calculationFinish.has_value())
        signalSemaphores.push_back(calculationFinish.value()._data->semaphores[manager.getCurrentBufferIdx()]);

    vk::PresentInfoKHR presentInfo;
    vk::SwapchainKHR swapchains[] = { swapchain._data->swapChain };

    presentInfo.waitSemaphoreCount = signalSemaphores.size();
    presentInfo.pWaitSemaphores = signalSemaphores.data();
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapchains;
    presentInfo.pImageIndices = &imageIndex;
    presentInfo.pResults = nullptr;
    inFlightFence->wait();
    auto result = manager._data->presentQueue.presentKHR(presentInfo);
    if (vk::Result::eSuccess != result) {
        loggingln("Warning: Missing present queue!! : ", to_string(result));
        return vsl::DefaultPhase::SubmitResult::WantRecreateSwapchain;
    }
    return vsl::DefaultPhase::SubmitResult::Success;
}

VSL_NAMESPACE::DefaultPhase::~DefaultPhase() {
    submit();
}

std::uint32_t vsl::DefaultPhase::getCurrentIndex() {
    return this->manager.getCurrentBufferIdx();
}

VSL_NAMESPACE::ComputePhase::ComputePhase(CommandManager _manager,
                                          std::optional<SemaphoreHolder> nextImageAvailable,
                                          std::optional<SemaphoreHolder> calculationFinish,
                                          std::optional<FenceHolder> inFlightFence,
                                          bool postponeSetup)
        : manager(_manager)
        , nextImageAvailable(nextImageAvailable)
        , calculationFinish(calculationFinish)
        , inFlightFence(inFlightFence) {
    if (not postponeSetup)
        setup();
}

vsl::ComputePhase::~ComputePhase() {
    submit();
}

void vsl::ComputePhase::setup() {
    if (state != State::BeforeSetup)
        return;

    auto manager = this->manager._data;
    auto currentFrame = manager->commandBuffer->currentBufferIdx;

    if (inFlightFence.has_value()) {
        inFlightFence.value().wait(currentFrame);
        inFlightFence.value().reset(currentFrame);
    }

    vk::CommandBufferBeginInfo beginInfo;
    // Optional beginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
    beginInfo.pInheritanceInfo = nullptr; // Optional

    manager->commandBuffer->commandBuffers[currentFrame].reset();
    manager->commandBuffer->commandBuffers[currentFrame].begin(beginInfo);

    state = State::WaitSubmit;
}

vsl::DefaultPhase::SubmitResult vsl::ComputePhase::submit() {
    if (state != State::WaitSubmit)
        return vsl::DefaultPhase::SubmitResult::Failed;

    auto manager = this->manager._data;
    auto currentFrame = manager->commandBuffer->currentBufferIdx;

    manager->commandBuffer->commandBuffers[currentFrame].end();

    std::vector<vk::Semaphore> waitSemaphores, signalSemaphores;
    if (nextImageAvailable.has_value())
        waitSemaphores.push_back(nextImageAvailable.value()._data->semaphores[currentFrame]);
    if (calculationFinish.has_value())
        signalSemaphores.push_back(calculationFinish.value()._data->semaphores[currentFrame]);

    vk::PipelineStageFlags waitStages[] = { vk::PipelineStageFlagBits::eColorAttachmentOutput };

    vk::SubmitInfo submitInfo;
    submitInfo.waitSemaphoreCount = waitSemaphores.size();
    submitInfo.pWaitSemaphores = waitSemaphores.data();
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &manager->commandBuffer->commandBuffers[currentFrame];
    submitInfo.signalSemaphoreCount = signalSemaphores.size();
    submitInfo.pSignalSemaphores = signalSemaphores.data();

    if (inFlightFence.has_value())
        manager->graphicsQueue.submit({ submitInfo }, inFlightFence.value()._data->fences[currentFrame]);
    else
        manager->graphicsQueue.submit({ submitInfo });

    state = State::FinishSubmit;
    return vsl::DefaultPhase::SubmitResult::Success;
}

VSL_NAMESPACE::DefaultPhaseStream VSL_NAMESPACE::ComputePhase::operator<<(std::shared_ptr<command::__Command> cmd)
{
    DefaultPhaseStream op{ this->manager };
    op << cmd;
    return op;
}

vsl::DefaultPhaseStream vsl::ComputePhase::operator<<(std::shared_ptr<command::__DelegateCommand> cmd) {
    DefaultPhaseStream op{ this->manager };
    op << cmd;
    return op;
}

VSL_NAMESPACE::DefaultPhaseStream& VSL_NAMESPACE::DefaultPhaseStream::operator<<(std::shared_ptr<command::__Command> cmd)
{
    resolveHolderRequirePtr(cmd);
    cmd->invoke(manager.getPool(), manager.getBuffer(), manager);
    return *this;
}

VSL_NAMESPACE::DefaultPhaseStream& VSL_NAMESPACE::DefaultPhaseStream::operator<<(std::shared_ptr<command::__DelegateCommand> cmd)
{
    resolveHolderRequirePtr(cmd);
    cmd->invoke(*this, manager.getPool(), manager.getBuffer(), manager);
    return *this;
}

std::uint32_t VSL_NAMESPACE::ComputePhase::getImageIndex() {
    return imageIndex;
}