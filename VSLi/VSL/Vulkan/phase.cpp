//
// Created by morimoto_hibiki on 2025/07/30.
//
#ifdef _MSC_VER
#include "pch.h"
#endif
#include "../define.h"

#include "_pimpls.h"
#include "../exceptions.h"

#include "phase.hpp"

VSL_NAMESPACE::DefaultPhase::DefaultPhase(CommandManager _manager, /*SwapchainAccessor swapchain,*/
                                          std::optional<SemaphoreHolder> nextImageAvailable,
                                          std::optional<SemaphoreHolder> calculationFinish,
                                          std::optional<FenceHolder> inFlightFence)
        : manager(_manager)
        // , swapchain(swapchain)
        , nextImageAvailable(nextImageAvailable)
        , calculationFinish(calculationFinish)
        , inFlightFence(inFlightFence) {
    auto manager = this->manager._data;
    auto currentFrame = manager->commandBuffer->currentBufferIdx;

    if (inFlightFence.has_value()) {
        inFlightFence.value().wait(currentFrame);
        inFlightFence.value().reset(currentFrame);
    }

    /* FIXME: Result check
    if (nextImageAvailable.has_value())
        imageIndex = manager->device->device.acquireNextImageKHR(swapchain._data->swapChain, UINT64_MAX
                , nextImageAvailable.value()._data->
                        semaphores[currentFrame],
                                                                 nullptr).value;
    else
        imageIndex = manager->device->device.acquireNextImageKHR(swapchain._data->swapChain, UINT64_MAX, nullptr, nullptr).value;
    */

    vk::CommandBufferBeginInfo beginInfo;
    // Optional beginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
    beginInfo.pInheritanceInfo = nullptr; // Optional

    manager->commandBuffer->commandBuffers[currentFrame].reset();
    manager->commandBuffer->commandBuffers[currentFrame].begin(beginInfo);
}

VSL_NAMESPACE::DefaultPhaseStreamOperator VSL_NAMESPACE::DefaultPhase::operator<<(std::shared_ptr<command::__Command> cmd)
{
    DefaultPhaseStreamOperator op{ this };
    op << cmd;
    return op;
}

VSL_NAMESPACE::DefaultPhaseStreamOperator& VSL_NAMESPACE::DefaultPhaseStreamOperator::operator<<(std::shared_ptr<command::__Command> cmd)
{
    resolveHolderRequirePtr(cmd);
    cmd->invoke(parent->manager.getPool(), parent->manager.getBuffer(), parent->manager);
    return *this;
}

VSL_NAMESPACE::DefaultPhaseStreamOperator& VSL_NAMESPACE::DefaultPhaseStreamOperator::operator<<(std::shared_ptr<command::__Manipulator> manip)
{
    resolveHolderRequirePtr(manip);
    manip->manipulate(this, parent->manager.getPool(), parent->manager.getBuffer(), parent->manager);
    return *this;
}

std::uint32_t VSL_NAMESPACE::DefaultPhase::getImageIndex() {
    return imageIndex;
}


VSL_NAMESPACE::DefaultPhase::~DefaultPhase() {
    auto manager = this->manager._data;
    auto currentFrame = manager->commandBuffer->currentBufferIdx;

    manager->commandBuffer->commandBuffers[currentFrame].end();

    std::uint32_t imageIdx = getImageIndex();

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

    /* TODO
    vk::PresentInfoKHR presentInfo;
    vk::SwapchainKHR swapchains[] = { swapchain._data->swapChain };

    presentInfo.waitSemaphoreCount = signalSemaphores.size();
    presentInfo.pWaitSemaphores = signalSemaphores.data();
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapchains;
    presentInfo.pImageIndices = &imageIdx;
    presentInfo.pResults = nullptr;

    manager->presentQueue.presentKHR(presentInfo);
     */
}
/*

VSL_NAMESPACE::ComputePhase::ComputePhase(CommandManager _manager,
                                          std::optional<SemaphoreHolder> nextImageAvailable,
                                          std::optional<SemaphoreHolder> calculationFinish,
                                          std::optional<FenceHolder> inFlightFence)
        : manager(_manager)
        , nextImageAvailable(nextImageAvailable)
        , calculationFinish(calculationFinish)
        , inFlightFence(inFlightFence) {
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
}

VSL_NAMESPACE::ComputePhaseStreamOperator VSL_NAMESPACE::ComputePhase::operator<<(std::shared_ptr<command::__Command> cmd)
{
    ComputePhaseStreamOperator op{ this };
    op << cmd;
    return op;
}

VSL_NAMESPACE::ComputePhaseStreamOperator& VSL_NAMESPACE::ComputePhaseStreamOperator::operator<<(std::shared_ptr<command::__Command> cmd)
{
    resolveHolderRequirePtr(cmd);
    cmd->invoke(parent->manager.getPool(), parent->manager.getBuffer(), parent->manager);
    return *this;
}

VSL_NAMESPACE::ComputePhaseStreamOperator& VSL_NAMESPACE::ComputePhaseStreamOperator::operator<<(std::shared_ptr<command::__Manipulator> manip)
{
    resolveHolderRequirePtr(manip);
    manip->manipulate(this, parent->manager.getPool(), parent->manager.getBuffer(), parent->manager);
    return *this;
}

template<command::is_command T>
vsl::ComputePhaseStreamOperator &vsl::ComputePhaseStreamOperator::operator<<(T
cmd){
return <#initializer#>;
}

std::uint32_t VSL_NAMESPACE::ComputePhase::getImageIndex() {
    return imageIndex;
}


VSL_NAMESPACE::ComputePhase::~ComputePhase() {
    auto manager = this->manager._data;
    auto currentFrame = manager->commandBuffer->currentBufferIdx;

    manager->commandBuffer->commandBuffers[currentFrame].end();

    std::uint32_t imageIdx = getImageIndex();

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

    vk::PresentInfoKHR presentInfo;
    vk::SwapchainKHR swapchains[] = { swapchain._data->swapChain };

    presentInfo.waitSemaphoreCount = signalSemaphores.size();
    presentInfo.pWaitSemaphores = signalSemaphores.data();
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapchains;
    presentInfo.pImageIndices = &imageIdx;
    presentInfo.pResults = nullptr;

    manager->presentQueue.presentKHR(presentInfo);
}
 */