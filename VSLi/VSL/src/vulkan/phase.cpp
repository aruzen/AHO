//
// Created by morimoto_hibiki on 2025/07/30.
//
#ifdef _MSC_VER
#include <VSL/Vulkan/pch.hpp>
#endif
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

    auto currentFrame = manager._data->commandBuffer->currentBufferIdx;
    // FIXME: Result check
    if (nextImageAvailable.has_value())
        imageIndex = manager._data->device->device.acquireNextImageKHR(swapchain._data->swapChain, UINT64_MAX
                , nextImageAvailable.value()._data->
                        semaphores[currentFrame],nullptr).value;
    else
        imageIndex = manager._data->device->device.acquireNextImageKHR(swapchain._data->swapChain, UINT64_MAX, nullptr, nullptr).value;

    ComputePhase::setup();
}

void vsl::DefaultPhase::submit() {
    if (state != State::WaitSubmit)
        return;
    ComputePhase::submit();

    std::uint32_t imageIdx = getImageIndex();
    std::vector<vk::Semaphore> signalSemaphores;
    if (calculationFinish.has_value())
        signalSemaphores.push_back(calculationFinish.value()._data->semaphores[manager.getCurrentBufferIdx()]);

    vk::PresentInfoKHR presentInfo;
    vk::SwapchainKHR swapchains[] = { swapchain._data->swapChain };

    presentInfo.waitSemaphoreCount = signalSemaphores.size();
    presentInfo.pWaitSemaphores = signalSemaphores.data();
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapchains;
    presentInfo.pImageIndices = &imageIdx;
    presentInfo.pResults = nullptr;

    auto result = manager._data->presentQueue.presentKHR(presentInfo);
    if (vk::Result::eSuccess != result)
        loggingln("Warning: Missing present queue!!");
}

VSL_NAMESPACE::DefaultPhase::~DefaultPhase() {
    submit();
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

void vsl::ComputePhase::submit() {
    if (state != State::WaitSubmit)
        return;

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
    submitInfo.waitSemaphoreCount = (std::uint32_t)waitSemaphores.size();
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
}

VSL_NAMESPACE::DefaultPhaseStream VSL_NAMESPACE::ComputePhase::operator<<(std::shared_ptr<command::__Command> cmd)
{
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

std::uint32_t VSL_NAMESPACE::ComputePhase::getImageIndex() {
    return imageIndex;
}