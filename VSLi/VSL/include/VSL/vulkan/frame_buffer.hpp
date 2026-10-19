#pragma once
#include "render_pass.hpp"
#include "swapchain.hpp"

namespace VSL_NAMESPACE {
    struct FrameBufferAccessor {
        std::shared_ptr<_impl::FrameBuffer_impl> _data;
    };
    struct AttachmentImage {
        AttachmentId attachment;
        ImageAccessor image;
    };
    struct FrameBuffer : FrameBufferAccessor {
        // 画像はRenderPassDescription::attachmentsと同じ順序で指定する。
        // パスと画像を保持する。Phaseを介さず記録する場合は、GPUの完了まで呼び出し側で保持する。
        FrameBuffer(RenderPassAccessor pass, std::uint32_t width, std::uint32_t height,
                    std::vector<ImageAccessor> images, std::uint32_t layers = 1);
        // AttachmentIdで対応付けるため、配列の順序は問わない。全アタッチメントの指定が必要。
        static FrameBuffer fromAttachments(RenderPassAccessor pass, std::uint32_t width, std::uint32_t height,
                                           std::vector<AttachmentImage> images, std::uint32_t layers = 1);
        FrameBuffer(SwapchainAccessor swapchain, RenderPass render_pass);
        FrameBuffer(SwapchainAccessor swapchain, IDPickingRenderPass render_pass);
    };
} // namespace VSL_NAMESPACE
