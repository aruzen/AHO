#include "pch.h"
#include "../define.h"

#include "_pimpls.h"
#include "../exceptions.h"

#include "frame_buffer.h"

template<typename D>
VSL_NAMESPACE::FrameBuffer<D>::FrameBuffer(Swapchain swapchain, View<D> view, RenderPass render_pass)
{
    _data = std::shared_ptr<VSL_NAMESPACE::_impl::FrameBuffer_impl>(new VSL_NAMESPACE::_impl::FrameBuffer_impl);
    _data->device = swapchain._data->device;

    _data->swapChainFramebuffers.resize(swapchain._data->swapChainImages.size());
    for (size_t i = 0; i < swapchain._data->swapChainImages.size(); i++) {
        vk::FramebufferCreateInfo framebufferInfo{};
        framebufferInfo.renderPass = render_pass._data->renderPass;
        framebufferInfo.attachmentCount = (uint32_t)view._data->swapChainImageViews.size();
        framebufferInfo.pAttachments = view._data->swapChainImageViews.data();
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

template struct vsl::FrameBuffer<>;