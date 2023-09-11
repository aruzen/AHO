#include "pch.h"
#include "render_pass.h"

#include "_pimpls.h"

VSL_NAMESPACE::RenderPass::RenderPass(SwapchainAcsessor swapchain)
{
    _data = std::shared_ptr<VSL_NAMESPACE::_impl::RenderPass_impl>(new VSL_NAMESPACE::_impl::RenderPass_impl);
    _data->device = swapchain._data->device;

    vk::AttachmentDescription colorAttachment;
    colorAttachment.format = swapchain._data->swapChainImageFormat;
    colorAttachment.samples = vk::SampleCountFlagBits::e1;
    colorAttachment.loadOp = vk::AttachmentLoadOp::eClear;
    colorAttachment.storeOp = vk::AttachmentStoreOp::eStore;
    colorAttachment.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
    colorAttachment.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
    colorAttachment.initialLayout = vk::ImageLayout::eUndefined;
    colorAttachment.finalLayout = vk::ImageLayout::ePresentSrcKHR;

    vk::AttachmentReference colorAttachmentRef;
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = vk::ImageLayout::eColorAttachmentOptimal;

    vk::SubpassDescription subpass;
    subpass.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;

    vk::RenderPassCreateInfo renderPassInfo;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &colorAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;

    _data->renderPass = _data->device->device.createRenderPass(renderPassInfo);
}

VSL_NAMESPACE::_impl::RenderPass_impl::~RenderPass_impl() {
    device->device.destroyRenderPass(renderPass);
}