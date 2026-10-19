#include <VSL/vulkan/frame_buffer.hpp>
#include <VSL/vulkan/_pimpls.hpp>
#include "image_format.hpp"
#include <stdexcept>

namespace VSL_NAMESPACE {
    namespace {
        std::shared_ptr<_impl::FrameBuffer_impl> make_data(RenderPassAccessor pass, std::uint32_t width,
                                                           std::uint32_t height, std::uint32_t layers) {
            if (!pass._data || !width || !height || !layers)
                throw std::invalid_argument("Invalid framebuffer pass/extent/layers");
            auto data = std::make_shared<_impl::FrameBuffer_impl>();
            data->device = pass._data->device;
            data->renderPass = pass._data;
            data->extent = {width, height};
            data->layers = layers;
            return data;
        }
        void append(_impl::FrameBuffer_impl& target, const std::vector<ImageAccessor>& images) {
            const auto& desc = *target.renderPass->description;
            if (images.size() != desc.attachments.size())
                throw std::invalid_argument("Framebuffer attachment count mismatch");
            std::vector<vk::ImageView> views;
            std::vector<std::shared_ptr<_impl::Image_impl>> retained;
            for (std::size_t i = 0; i < images.size(); ++i) {
                const auto& image = images[i]._data;
                const auto& a = desc.attachments[i];
                if (!image || !image->view || image->device != target.device ||
                    image->format != static_cast<vk::Format>(a.format.type()) || image->samples != a.samples ||
                    image->width < target.extent.width || image->height < target.extent.height ||
                    image->count < target.layers)
                    throw std::invalid_argument("Framebuffer attachment device/format/sample/extent mismatch");
                auto require = [&](ImageType usage) {
                    if (!contain(image->usage, usage))
                        throw std::invalid_argument("Framebuffer image missing required usage");
                };
                for (const auto& s : desc.subpasses) {
                    for (auto r : s.colors)
                        if (r.attachment == i)
                            require(ImageType::ColorAttachment);
                    for (auto r : s.resolves)
                        if (r.attachment == i)
                            require(ImageType::ColorAttachment);
                    for (auto r : s.inputs)
                        if (r.attachment == i)
                            require(ImageType::InputAttachment);
                    if (s.depthStencil && s.depthStencil->attachment == i)
                        require(ImageType::DepthStencilAttachment);
                }
                views.push_back(image->view);
                retained.push_back(image);
            }
            vk::FramebufferCreateInfo info{.renderPass = target.renderPass->renderPass,
                                           .attachmentCount = static_cast<std::uint32_t>(views.size()),
                                           .pAttachments = views.data(),
                                           .width = target.extent.width,
                                           .height = target.extent.height,
                                           .layers = target.layers};
            target.swapChainFramebuffers.push_back(target.device->device.createFramebuffer(info));
            target.attachments.push_back(std::move(retained));
        }
    } // namespace
    FrameBuffer::FrameBuffer(RenderPassAccessor pass, std::uint32_t width, std::uint32_t height,
                             std::vector<ImageAccessor> images, std::uint32_t layers) {
        _data = make_data(pass, width, height, layers);
        append(*_data, images);
    }
    FrameBuffer::FrameBuffer(SwapchainAccessor swapchain, RenderPass pass) {
        if (!swapchain._data)
            throw std::invalid_argument("Swapchain required");
        _data =
            make_data(pass, swapchain._data->swapChainExtent.width, swapchain._data->swapChainExtent.height, 1);
        _data->swapchain = swapchain._data;
        for (auto image : swapchain.images)
            append(*_data, {image});
    }
    FrameBuffer::FrameBuffer(SwapchainAccessor swapchain, IDPickingRenderPass pass) {
        if (!swapchain._data || swapchain.images.size() != pass.picking_buffer.size())
            throw std::invalid_argument("Picking image count must match swapchain");
        _data =
            make_data(pass, swapchain._data->swapChainExtent.width, swapchain._data->swapChainExtent.height, 1);
        _data->swapchain = swapchain._data;
        for (std::size_t i = 0; i < swapchain.images.size(); ++i)
            append(*_data, {swapchain.images[i], pass.picking_buffer[i]});
    }
    _impl::FrameBuffer_impl::~FrameBuffer_impl() {
        if (device)
            for (auto fb : swapChainFramebuffers)
                device->device.destroyFramebuffer(fb);
    }
} // namespace VSL_NAMESPACE
