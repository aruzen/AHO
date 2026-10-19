#pragma once
#include "render_pass.hpp"
#include "swapchain.hpp"

namespace VSL_NAMESPACE {
struct FrameBufferAccessor {
    std::shared_ptr<_impl::FrameBuffer_impl> _data;
};
struct FrameBuffer : FrameBufferAccessor {
    // 画像はRenderPassDescription::attachmentsと同じ順序で指定する。
    // パスと画像を保持する。呼び出し側はGPUの完了までこのオブジェクトを保持する。
    FrameBuffer(RenderPassAccessor pass, std::uint32_t width, std::uint32_t height,
                std::vector<ImageAccessor> images, std::uint32_t layers = 1);
    FrameBuffer(SwapchainAccessor swapchain, RenderPass render_pass);
    FrameBuffer(SwapchainAccessor swapchain, IDPickingRenderPass render_pass);
};
}
