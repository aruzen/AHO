#include <VSL/vulkan/frame_buffer.hpp>
#include <VSL/vulkan/_pimpls.hpp>
#include "image_format.hpp"
#include <stdexcept>

namespace VSL_NAMESPACE {
    namespace {
        std::shared_ptr<impl::FrameBufferImpl> make_data(RenderPassAccessor pass, std::uint32_t width,
                                                           std::uint32_t height, std::uint32_t layers) {
            if (!pass.data_ || !width || !height || !layers)
                throw std::invalid_argument("Invalid framebuffer pass/extent/layers");
            auto data = std::make_shared<impl::FrameBufferImpl>();
            data->device = pass.data_->device;
            data->render_pass = pass.data_;
            data->extent = {width, height};
            data->layers = layers;
            return data;
        }
        void append(impl::FrameBufferImpl& target, const std::vector<ImageAccessor>& images) {
            const auto& desc = *target.render_pass->description;
            if (images.size() != desc.attachments.size())
                throw std::invalid_argument("Framebuffer attachment count mismatch");
            std::vector<vk::ImageView> views;
            std::vector<std::shared_ptr<impl::ImageImpl>> retained;
            for (std::size_t i = 0; i < images.size(); ++i) {
                const auto& image = images[i].data_;
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
                            require(ImageType::COLOR_ATTACHMENT);
                    for (auto r : s.resolves)
                        if (r.attachment == i)
                            require(ImageType::COLOR_ATTACHMENT);
                    for (auto r : s.inputs)
                        if (r.attachment == i)
                            require(ImageType::INPUT_ATTACHMENT);
                    if (s.depth_stencil && s.depth_stencil->attachment == i)
                        require(ImageType::DEPTH_STENCIL_ATTACHMENT);
                }
                views.push_back(image->view);
                retained.push_back(image);
            }
            vk::FramebufferCreateInfo info{.renderPass = target.render_pass->render_pass,
                                           .attachmentCount = static_cast<std::uint32_t>(views.size()),
                                           .pAttachments = views.data(),
                                           .width = target.extent.width,
                                           .height = target.extent.height,
                                           .layers = target.layers};
            target.swap_chain_framebuffers.push_back(target.device->device.createFramebuffer(info));
            target.attachments.push_back(std::move(retained));
        }
    } // namespace
    FrameBuffer::FrameBuffer(RenderPassAccessor pass, std::uint32_t width, std::uint32_t height,
                             std::vector<ImageAccessor> images, std::uint32_t layers) {
        data_ = make_data(pass, width, height, layers);
        append(*data_, images);
    }
    FrameBuffer FrameBuffer::FromAttachments(RenderPassAccessor pass, std::uint32_t width, std::uint32_t height,
                                             std::vector<AttachmentImage> bindings, std::uint32_t layers) {
        const auto count = pass.description().attachments.size();
        if (bindings.size() != count)
            throw std::invalid_argument("Every framebuffer attachment must be bound");
        std::vector<ImageAccessor> images(count);
        std::vector<bool> bound(count);
        for (auto& binding : bindings) {
            if (binding.attachment >= count)
                throw std::out_of_range("Framebuffer attachment id");
            if (bound[binding.attachment])
                throw std::invalid_argument("Duplicate framebuffer attachment id");
            bound[binding.attachment] = true;
            images[binding.attachment] = std::move(binding.image);
        }
        return FrameBuffer(pass, width, height, std::move(images), layers);
    }
    FrameBuffer::FrameBuffer(SwapchainAccessor swapchain, RenderPass pass) {
        if (!swapchain.data_)
            throw std::invalid_argument("Swapchain required");
        data_ =
            make_data(pass, swapchain.data_->swap_chain_extent.width, swapchain.data_->swap_chain_extent.height, 1);
        data_->swapchain = swapchain.data_;
        for (auto image : swapchain.images)
            append(*data_, {image});
    }
    FrameBuffer::FrameBuffer(SwapchainAccessor swapchain, IDPickingRenderPass pass) {
        if (!swapchain.data_ || swapchain.images.size() != pass.picking_buffer.size())
            throw std::invalid_argument("Picking image count must match swapchain");
        data_ =
            make_data(pass, swapchain.data_->swap_chain_extent.width, swapchain.data_->swap_chain_extent.height, 1);
        data_->swapchain = swapchain.data_;
        for (std::size_t i = 0; i < swapchain.images.size(); ++i)
            append(*data_, {swapchain.images[i], pass.picking_buffer[i]});
    }
    impl::FrameBufferImpl::~FrameBufferImpl() {
        if (device)
            for (auto fb : swap_chain_framebuffers)
                device->device.destroyFramebuffer(fb);
    }
} // namespace VSL_NAMESPACE
