#pragma once
#include "render_pass.hpp"
#include "swapchain.hpp"

namespace VSL_NAMESPACE {
struct FrameBufferAccessor {
    std::shared_ptr<_impl::FrameBuffer_impl> _data;
};
struct FrameBuffer : FrameBufferAccessor {
    // One framebuffer; image order matches RenderPassDescription::attachments.
    // Retains the pass and image handles. Caller keeps this alive until GPU completion.
    FrameBuffer(RenderPassAccessor pass, std::uint32_t width, std::uint32_t height,
                std::vector<ImageAccessor> images, std::uint32_t layers = 1);
    FrameBuffer(SwapchainAccessor swapchain, RenderPass render_pass);
    FrameBuffer(SwapchainAccessor swapchain, IDPickingRenderPass render_pass);
};
} // namespace VSL_NAMESPACE
