#include <VSL/lifetime/swapchain_access.hpp>
#include <VSL/lifetime/owner_validation.hpp>
#include <VSL/lifetime/graphics_access.hpp>
#include <VSL/lifetime/identity_access.hpp>
#include <VSL/lifetime/image_access.hpp>
#include <VSL/lifetime/device_owner.hpp>
#include <VSL/vulkan/frame_buffer.hpp>
#include <VSL/lifetime/frame_buffer_storage.hpp>
#include <VSL/lifetime/owner_access.hpp>
#include <stdexcept>

namespace {
    using VSL_NAMESPACE::lifetime::detail::SwapchainAccess;
}

namespace {
    using VSL_NAMESPACE::lifetime::detail::GraphicsAccess;
}

namespace {
    using VSL_NAMESPACE::lifetime::detail::ImageAccess;
}

namespace VSL_NAMESPACE {
    FrameBufferIdentity FrameBufferAccessor::identity() const {
        return lifetime::detail::IdentityAccess::From<FrameBufferIdentity>(_owner.get());
    }
    namespace {
        FrameBufferAccessor make_frame_buffer(RenderPassAccessor pass, std::uint32_t width, std::uint32_t height,
                                              std::uint32_t layers, const std::vector<std::vector<ImageAccessor>>& rows, lifetime::OwnerRef auxiliary = {});
        lifetime::OwnerRef swapchain_owner(const SwapchainAccessor& swapchain, const RenderPassAccessor& pass);
        FrameBufferAccessor make_frame_buffer(RenderPassAccessor pass, std::uint32_t width, std::uint32_t height,
                                              std::uint32_t layers, const std::vector<std::vector<ImageAccessor>>& rows, lifetime::OwnerRef auxiliary) {
            const auto device = pass.get_device();
            operation::FrameBufferInput input{GraphicsAccess::Owned(pass).get(), width, height, layers, {}};
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
            return GraphicsAccess::FromShared(lifetime::make_heap_frame_buffer(device, std::move(input), pass.owner_ref(), std::move(dependencies)));
        }
        lifetime::OwnerRef swapchain_owner(const SwapchainAccessor& swapchain, const RenderPassAccessor& pass) {
            if (!SwapchainAccess::Owned(swapchain) || !SwapchainAccess::Owned(swapchain).use_count() || SwapchainAccess::Owned(swapchain)->device != pass.get_device().data_.get())
                throw std::invalid_argument("Framebuffer swapchain requires an owner on the pass device");
            return swapchain.owner_ref();
        }
    } // namespace
    RenderPassAccessor FrameBufferAccessor::get_render_pass() const {
        if (!_owner || !_owner.use_count() || !_owner->render_pass || !_owner->owner_context)
            throw std::invalid_argument("Framebuffer requires an owning pass context");
        auto owner = _owner->owner_context->pass.lock();
        if (!owner || owner.get() != _owner->render_pass)
            throw std::logic_error("Framebuffer pass owner is unavailable");
        return GraphicsAccess::FromOwned(std::move(owner));
    }
    LogicalDeviceAccessor FrameBufferAccessor::get_device() const {
        if (!_owner || !_owner.use_count() || !_owner->device || _owner->swap_chain_framebuffers.empty() || !_owner->swap_chain_framebuffers.front())
            throw std::invalid_argument("Framebuffer requires a live owner");
        return lifetime::device_from_child(_owner);
    }
    lifetime::OwnerRef FrameBufferAccessor::owner_ref() const {
        lifetime::detail::require_live_owner(_owner);
        return _owner;
    }
    size_t FrameBufferAccessor::frame_count() const {
        if (!_owner)
            return 0;
        if (!_owner.use_count())
            throw std::invalid_argument("Framebuffer requires an owner");
        return _owner->swap_chain_framebuffers.size();
    }
    FrameBuffer::FrameBuffer(RenderPassAccessor pass, std::uint32_t width, std::uint32_t height,
                             std::vector<ImageAccessor> images, std::uint32_t layers) {
        FrameBufferAccessor::operator=(make_frame_buffer(std::move(pass), width, height, layers, {std::move(images)}));
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
        rows.reserve(swapchain.get_swap_image_size());
        for (const auto& image : swapchain.images())
            rows.push_back({image});
        FrameBufferAccessor::operator=(make_frame_buffer(pass, SwapchainAccess::Owned(swapchain)->swap_chain_extent.width, SwapchainAccess::Owned(swapchain)->swap_chain_extent.height, 1, rows, owner));
    }
    FrameBuffer::FrameBuffer(SwapchainAccessor swapchain, IDPickingRenderPass pass) {
        const auto owner = swapchain_owner(swapchain, pass);
        if (swapchain.get_swap_image_size() != pass.image_count())
            throw std::invalid_argument("Picking image count must match swapchain");
        std::vector<std::vector<ImageAccessor>> rows;
        rows.reserve(swapchain.get_swap_image_size());
        for (size_t i = 0; i < swapchain.get_swap_image_size(); ++i)
            rows.push_back({swapchain.image(i), pass.picking_image(i)});
        FrameBufferAccessor::operator=(make_frame_buffer(pass, SwapchainAccess::Owned(swapchain)->swap_chain_extent.width, SwapchainAccess::Owned(swapchain)->swap_chain_extent.height, 1, rows, owner));
    }
} // namespace VSL_NAMESPACE

VSL_NAMESPACE::FrameBufferAccessor VSL_NAMESPACE::lifetime::detail::GraphicsAccess::FromShared(std::shared_ptr<impl::FrameBufferImpl> owner) {
    const auto* node = owner ? owner->owner_node : nullptr;
    return FromOwned(owned_ref(std::move(owner), node));
}
