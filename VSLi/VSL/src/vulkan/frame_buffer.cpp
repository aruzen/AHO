#include <VSL/lifetime/image_access.hpp>
#include <VSL/lifetime/device_owner.hpp>
#include <VSL/vulkan/frame_buffer.hpp>
#include <VSL/lifetime/frame_buffer_storage.hpp>
#include <VSL/lifetime/owner_access.hpp>
#include <stdexcept>

namespace {
    using VSL_NAMESPACE::lifetime::detail::ImageAccess;
}

namespace VSL_NAMESPACE {
    namespace {
        std::shared_ptr<impl::FrameBufferImpl> make_frame_buffer(RenderPassAccessor pass, std::uint32_t width, std::uint32_t height,
                                                                 std::uint32_t layers, const std::vector<std::vector<ImageAccessor>>& rows, lifetime::OwnerRef auxiliary = {});
        lifetime::OwnerRef swapchain_owner(const SwapchainAccessor& swapchain, const RenderPassAccessor& pass);
        std::shared_ptr<impl::FrameBufferImpl> make_frame_buffer(RenderPassAccessor pass, std::uint32_t width, std::uint32_t height,
                                                                 std::uint32_t layers, const std::vector<std::vector<ImageAccessor>>& rows, lifetime::OwnerRef auxiliary) {
            const auto device = pass.get_device();
            operation::FrameBufferInput input{pass.data_.get(), width, height, layers, {}};
            input.attachments.reserve(rows.size());
            std::vector<lifetime::OwnerRef> dependencies;
            lifetime::detail::retain_owner(dependencies, std::move(auxiliary));
            for (const auto& row : rows) {
                std::vector<impl::ImageImpl*> images;
                images.reserve(row.size());
                for (const auto& image : row) {
                    if (!ImageAccess::Owned(image) || !ImageAccess::Owned(image).use_count())
                        throw std::invalid_argument("Framebuffer images require owning accessors");
                    lifetime::detail::retain_owner(dependencies, ImageAccess::Owned(image));
                    images.push_back(ImageAccess::Owned(image).get());
                }
                input.attachments.push_back(std::move(images));
            }
            return lifetime::make_heap_frame_buffer(device, std::move(input), pass.owner_ref(), std::move(dependencies));
        }
        lifetime::OwnerRef swapchain_owner(const SwapchainAccessor& swapchain, const RenderPassAccessor& pass) {
            if (!swapchain.data_ || !swapchain.data_.use_count() || swapchain.data_->device != pass.get_device().data_.get())
                throw std::invalid_argument("Framebuffer swapchain requires an owner on the pass device");
            return swapchain.owner_ref();
        }
    } // namespace
    RenderPassAccessor FrameBufferAccessor::get_render_pass() const {
        if (!data_ || !data_.use_count() || !data_->render_pass || !data_->owner_context)
            throw std::invalid_argument("Framebuffer requires an owning pass context");
        auto owner = data_->owner_context->pass.lock();
        if (!owner || owner.get() != data_->render_pass)
            throw std::logic_error("Framebuffer pass owner is unavailable");
        return {owner.share()};
    }
    LogicalDeviceAccessor FrameBufferAccessor::get_device() const {
        if (!data_ || !data_.use_count() || !data_->device || data_->swap_chain_framebuffers.empty() || !data_->swap_chain_framebuffers.front())
            throw std::invalid_argument("Framebuffer requires a live owner");
        return lifetime::device_from_child(data_);
    }
    lifetime::OwnerRef FrameBufferAccessor::owner_ref() const {
        if (!data_ || !data_.use_count())
            return {};
        return lifetime::detail::owned_ref(data_, data_->owner_node);
    }
    size_t FrameBufferAccessor::frame_count() const {
        if (!data_)
            return 0;
        if (!data_.use_count())
            throw std::invalid_argument("Framebuffer requires an owner");
        return data_->swap_chain_framebuffers.size();
    }
    FrameBuffer::FrameBuffer(RenderPassAccessor pass, std::uint32_t width, std::uint32_t height,
                             std::vector<ImageAccessor> images, std::uint32_t layers) {
        data_ = make_frame_buffer(std::move(pass), width, height, layers, {std::move(images)});
    }
    FrameBuffer FrameBuffer::FromAttachments(RenderPassAccessor pass, std::uint32_t width, std::uint32_t height,
                                             std::vector<AttachmentImage> bindings, std::uint32_t layers) {
        (void)pass.get_device();
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
        return FrameBuffer(std::move(pass), width, height, std::move(images), layers);
    }
    FrameBuffer::FrameBuffer(SwapchainAccessor swapchain, RenderPass pass) {
        const auto owner = swapchain_owner(swapchain, pass);
        std::vector<std::vector<ImageAccessor>> rows;
        rows.reserve(swapchain.images.size());
        for (const auto& image : swapchain.images)
            rows.push_back({image});
        data_ = make_frame_buffer(pass, swapchain.data_->swap_chain_extent.width, swapchain.data_->swap_chain_extent.height, 1, rows, owner);
    }
    FrameBuffer::FrameBuffer(SwapchainAccessor swapchain, IDPickingRenderPass pass) {
        const auto owner = swapchain_owner(swapchain, pass);
        if (swapchain.images.size() != pass.picking_buffer.size())
            throw std::invalid_argument("Picking image count must match swapchain");
        std::vector<std::vector<ImageAccessor>> rows;
        rows.reserve(swapchain.images.size());
        for (size_t i = 0; i < swapchain.images.size(); ++i)
            rows.push_back({swapchain.images[i], pass.picking_buffer[i]});
        data_ = make_frame_buffer(pass, swapchain.data_->swap_chain_extent.width, swapchain.data_->swap_chain_extent.height, 1, rows, owner);
    }
} // namespace VSL_NAMESPACE
