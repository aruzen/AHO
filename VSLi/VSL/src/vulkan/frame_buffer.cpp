#include <VSL/define.hpp>

#include <VSL/vulkan/_pimpls.hpp>
#include <VSL/exceptions.hpp>

#include <VSL/vulkan/frame_buffer.hpp>
#include <memory>

VSL_NAMESPACE::FrameBuffer::FrameBuffer(SwapchainAccessor swapchain, RenderPass render_pass)
{
    _data = std::make_shared<VSL_NAMESPACE::_impl::FrameBuffer_impl>();
    _data->device = swapchain._data->device;
    _data->swapchain = swapchain._data;

    _data->swapChainFramebuffers.resize(swapchain.images.size());
    for (size_t i = 0; i < swapchain.images.size(); i++) {
        vk::ImageView attachments[] = {
            swapchain.images[i]._data->view
        };
        vk::FramebufferCreateInfo framebufferInfo{};
        framebufferInfo.renderPass = render_pass._data->renderPass;
        framebufferInfo.attachmentCount = 1;
        framebufferInfo.pAttachments = attachments;
        framebufferInfo.width = swapchain._data->swapChainExtent.width;
        framebufferInfo.height = swapchain._data->swapChainExtent.height;
        framebufferInfo.layers = 1;

        _data->swapChainFramebuffers[i] = _data->device->device.createFramebuffer(framebufferInfo);
    }
}


VSL_NAMESPACE::FrameBuffer::FrameBuffer(SwapchainAccessor swapchain, IDPickingRenderPass render_pass)
{
    _data = std::make_shared<VSL_NAMESPACE::_impl::FrameBuffer_impl>();
    _data->device = swapchain._data->device;
    _data->swapchain = swapchain._data;

    _data->swapChainFramebuffers.resize(swapchain.images.size());
    for (size_t i = 0; i < swapchain.images.size(); i++) {
        vk::ImageView attachments[] = {
                swapchain.images[i]._data->view, render_pass.picking_buffer[i]._data->view
        };
        vk::FramebufferCreateInfo framebufferInfo{};
        framebufferInfo.renderPass = render_pass._data->renderPass;
        framebufferInfo.attachmentCount = std::size(attachments);
        framebufferInfo.pAttachments = attachments;
        framebufferInfo.width = swapchain._data->swapChainExtent.width;
        framebufferInfo.height = swapchain._data->swapChainExtent.height;
        framebufferInfo.layers = 1;

        _data->swapChainFramebuffers[i] = _data->device->device.createFramebuffer(framebufferInfo);

    }
}

VSL_NAMESPACE::_impl::FrameBuffer_impl::~FrameBuffer_impl()
{
    for (auto framebuffer : swapChainFramebuffers) {
        device->device.destroyFramebuffer(framebuffer);
    }
}
