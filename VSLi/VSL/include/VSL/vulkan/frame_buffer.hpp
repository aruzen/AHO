#pragma once
#include "render_pass.hpp"
#include "swapchain.hpp"

namespace VSL_NAMESPACE::lifetime::detail {
    struct GraphicsAccess;
}

namespace VSL_NAMESPACE {
    struct FrameBufferAccessor {
    private:
        friend struct lifetime::detail::GraphicsAccess;
        lifetime::OwnedRef<impl::FrameBufferImpl> _owner;
        explicit FrameBufferAccessor(lifetime::OwnedRef<impl::FrameBufferImpl> owner) : _owner(std::move(owner)) {}

    public:
        FrameBufferAccessor() = default;
        FrameBufferAccessor(const FrameBufferAccessor&) = default;
        FrameBufferAccessor(FrameBufferAccessor&&) noexcept = default;
        FrameBufferAccessor& operator=(FrameBufferAccessor replacement) noexcept {
            swap(replacement);
            return *this;
        }
        explicit operator bool() const noexcept { return bool(_owner); }
        void swap(FrameBufferAccessor& other) noexcept { _owner.swap(other._owner); }
        FrameBufferIdentity identity() const;
        RenderPassAccessor get_render_pass() const;
        LogicalDeviceAccessor get_device() const;
        lifetime::OwnerRef owner_ref() const;
        size_t frame_count() const;
    };
    struct AttachmentImage {
        AttachmentId attachment;
        ImageAccessor image;
    };
    struct FrameBuffer : FrameBufferAccessor {
        FrameBuffer(const FrameBuffer&) = default;
        FrameBuffer(FrameBuffer&&) noexcept = default;
        FrameBuffer& operator=(FrameBuffer replacement) noexcept {
            swap(replacement);
            return *this;
        }
        // 画像はRenderPassDescription::attachmentsと同じ順序で指定する。
        // パスと画像を保持する。Phaseを介さず記録する場合は、GPUの完了まで呼び出し側で保持する。
        FrameBuffer(RenderPassAccessor pass, std::uint32_t width, std::uint32_t height,
                    std::vector<ImageAccessor> images, std::uint32_t layers = 1);
        // AttachmentIdで対応付けるため、配列の順序は問わない。全アタッチメントの指定が必要。
        static FrameBuffer FromAttachments(RenderPassAccessor pass, std::uint32_t width, std::uint32_t height,
                                           std::vector<AttachmentImage> images, std::uint32_t layers = 1);
        FrameBuffer(SwapchainAccessor swapchain, RenderPass render_pass);
        FrameBuffer(SwapchainAccessor swapchain, IDPickingRenderPass render_pass);
    };
} // namespace VSL_NAMESPACE
