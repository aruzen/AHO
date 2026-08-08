#include <VSL/define.hpp>

#include <VSL/vulkan/_pimpls.hpp>
#include <VSL/exceptions.hpp>

#include <VSL/vulkan/frame_buffer.hpp>
#include <memory>
#include <source_location>

VSL_NAMESPACE::FrameBuffer::FrameBuffer(SwapchainAccessor swapchain, RenderPass render_pass)
{
    if (swapchain._data == nullptr || swapchain._data->device == nullptr || render_pass._data == nullptr || !render_pass._data->renderPass) {
        throw VSL_NAMESPACE::exceptions::RuntimeException(
                "FrameBuffer",
                "FrameBuffer creation requires a valid swapchain and render pass.",
                std::source_location::current());
    }
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


VSL_NAMESPACE::FrameBuffer::FrameBuffer(SwapchainAccessor swapchain, ReadableRenderPass render_pass)
{
    if (swapchain._data == nullptr || swapchain._data->device == nullptr || render_pass._data == nullptr || !render_pass._data->renderPass) {
        throw VSL_NAMESPACE::exceptions::RuntimeException(
                "FrameBuffer",
                "FrameBuffer creation requires a valid swapchain and render pass.",
                std::source_location::current());
    }
    _data = std::make_shared<VSL_NAMESPACE::_impl::FrameBuffer_impl>();
    _data->device = swapchain._data->device;
    _data->swapchain = swapchain._data;

    _data->swapChainFramebuffers.resize(swapchain.images.size());
    for (size_t i = 0; i < swapchain.images.size(); i++) {
        std::vector<vk::ImageView> attachments;
        attachments.push_back(swapchain.images[i]._data->view);
        for (size_t attachment = 0; attachment < render_pass.readable_buffers.size(); attachment++) {
            attachments.push_back(render_pass.readable_image(attachment, i)._data->view);
        }
        vk::FramebufferCreateInfo framebufferInfo{};
        framebufferInfo.renderPass = render_pass._data->renderPass;
        framebufferInfo.attachmentCount = attachments.size();
        framebufferInfo.pAttachments = attachments.data();
        framebufferInfo.width = swapchain._data->swapChainExtent.width;
        framebufferInfo.height = swapchain._data->swapChainExtent.height;
        framebufferInfo.layers = 1;

        _data->swapChainFramebuffers[i] = _data->device->device.createFramebuffer(framebufferInfo);

    }
}

VSL_NAMESPACE::_impl::FrameBuffer_impl::~FrameBuffer_impl()
{
    if (device == nullptr)
        return;
    for (auto framebuffer : swapChainFramebuffers) {
        if (framebuffer)
            device->device.destroyFramebuffer(framebuffer);
    }
}
