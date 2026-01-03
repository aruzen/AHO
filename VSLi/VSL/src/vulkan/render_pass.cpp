#include <VSL/vulkan/render_pass.hpp>

#include <VSL/vulkan/_pimpls.hpp>
#include <VSL/vulkan/image.hpp>

VSL_NAMESPACE::RenderPass::RenderPass(SwapchainAccessor swapchain)
{
    _data = std::shared_ptr<VSL_NAMESPACE::_impl::RenderPass_impl>(new VSL_NAMESPACE::_impl::RenderPass_impl);
    _data->device = swapchain._data->device;

    // TODO ここのAPIの提供ができてない
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

    vk::SubpassDependency dependency;
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    // dependency.srcAccessMask;
    dependency.dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    dependency.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;

    vk::RenderPassCreateInfo renderPassInfo;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &colorAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    _data->renderPass = _data->device->device.createRenderPass(renderPassInfo);
}


VSL_NAMESPACE::IDPickingRenderPass::IDPickingRenderPass(SwapchainAccessor swapchain) {
    _data = std::shared_ptr<VSL_NAMESPACE::_impl::RenderPass_impl>(new VSL_NAMESPACE::_impl::RenderPass_impl);
    _data->device = swapchain._data->device;

    // TODO ここのAPIの提供ができてない
    std::array<vk::AttachmentDescription, 2> attachments = {
            vk::AttachmentDescription{
                    .format = swapchain._data->swapChainImageFormat,
                    .samples = vk::SampleCountFlagBits::e1,
                    .loadOp = vk::AttachmentLoadOp::eClear,
                    .storeOp = vk::AttachmentStoreOp::eStore,
                    .stencilLoadOp = vk::AttachmentLoadOp::eDontCare,
                    .stencilStoreOp = vk::AttachmentStoreOp::eDontCare,
                    .initialLayout = vk::ImageLayout::eUndefined,
                    .finalLayout = vk::ImageLayout::ePresentSrcKHR,
            },
            vk::AttachmentDescription{
                    .format = vk::Format::eR32Uint,
                    .samples = vk::SampleCountFlagBits::e1,
                    .loadOp = vk::AttachmentLoadOp::eClear,
                    .storeOp = vk::AttachmentStoreOp::eStore,
                    .stencilLoadOp = vk::AttachmentLoadOp::eDontCare,
                    .stencilStoreOp = vk::AttachmentStoreOp::eDontCare,
                    .initialLayout = vk::ImageLayout::eUndefined,
                    .finalLayout = vk::ImageLayout::eColorAttachmentOptimal,
            }
    };
    std::array<vk::AttachmentReference, 2> colorAttachmentRefs = {
            vk::AttachmentReference{
                    .attachment = 0,
                    .layout = vk::ImageLayout::eColorAttachmentOptimal,
            },
            vk::AttachmentReference{
                    .attachment = 1,
                    .layout = vk::ImageLayout::eColorAttachmentOptimal,
            }
    };

    vk::SubpassDescription subpass;
    subpass.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
    subpass.colorAttachmentCount = colorAttachmentRefs.size();
    subpass.pColorAttachments = colorAttachmentRefs.data();

    vk::SubpassDependency dependency;
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    // dependency.srcAccessMask;
    dependency.dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    dependency.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;

    vk::RenderPassCreateInfo renderPassInfo;
    renderPassInfo.attachmentCount = attachments.size();
    renderPassInfo.pAttachments = attachments.data();
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    _data->renderPass = _data->device->device.createRenderPass(renderPassInfo);

    picking_buffer.reserve(swapchain.getSwapImageSize());
    for (size_t i = 0; i < swapchain.getSwapImageSize(); i++) {
        Image<ImageType::ColorAttachment | ImageType::TransferSrc> image(LogicalDeviceAccessor{_data->device},
                                                                         swapchain._data->extent.width,
                                                                         swapchain._data->extent.height,
                                                                         data_format::UnsignedInt);


        picking_buffer.push_back(image);
    }
}

VSL_NAMESPACE::_impl::RenderPass_impl::~RenderPass_impl() {
    device->device.destroyRenderPass(renderPass);
}
