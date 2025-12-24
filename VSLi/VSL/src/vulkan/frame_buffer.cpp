#ifdef _MSC_VER
#include <VSL/Vulkan/pch.hpp>
#endif
#include <VSL/define.hpp>

#include <VSL/vulkan/_pimpls.hpp>
#include <VSL/exceptions.hpp>

#include <VSL/vulkan/frame_buffer.hpp>

template<typename D>
VSL_NAMESPACE::FrameBuffer<D>::FrameBuffer(SwapchainAccessor swapchain, View<D> view, RenderPass render_pass)
{
    _data = std::shared_ptr<VSL_NAMESPACE::_impl::FrameBuffer_impl>(new VSL_NAMESPACE::_impl::FrameBuffer_impl);
    _data->device = swapchain._data->device;
    _data->swapchain = swapchain._data;

    _data->swapChainFramebuffers.resize(swapchain._data->swapChainImages.size());
    for (size_t i = 0; i < swapchain._data->swapChainImages.size(); i++) {
        vk::ImageView attachments[] = {
            view._data->swapChainImageViews[i]
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

void VSL_NAMESPACE::FrameBufferAccessor::setTargetFrame(std::uint32_t frameIdx) {
    _data->currentIndex = frameIdx;
}

VSL_NAMESPACE::_impl::FrameBuffer_impl::~FrameBuffer_impl()
{
    for (auto framebuffer : swapChainFramebuffers) {
        device->device.destroyFramebuffer(framebuffer);
    }
}

template struct vsl::FrameBuffer<>;
