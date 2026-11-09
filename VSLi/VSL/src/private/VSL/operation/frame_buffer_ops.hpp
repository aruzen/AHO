#pragma once
#include <VSL/vulkan/_pimpls.hpp>

namespace VSL_NAMESPACE::operation {
    struct FrameBufferInput {
        impl::RenderPassImpl* pass = nullptr;
        std::uint32_t width = 0, height = 0, layers = 1;
        // 行=native framebuffer、列=AttachmentId。所有は呼出側root。
        std::vector<std::vector<impl::ImageImpl*>> attachments;
    };
    // 構築呼出し中だけのnative境界。Implや記録経路には保存しない。
    struct FrameBufferNativeFactory {
        void* context = nullptr;
        vk::Framebuffer (*create)(void*, vk::Device, const vk::FramebufferCreateInfo&) = nullptr;
        void (*destroy)(void*, vk::Device, vk::Framebuffer) noexcept = nullptr;
    };
    class ScopedFrameBufferNativeFactory {
        FrameBufferNativeFactory _factory;
        const FrameBufferNativeFactory* _previous;

    public:
        explicit ScopedFrameBufferNativeFactory(FrameBufferNativeFactory factory) noexcept;
        ScopedFrameBufferNativeFactory(const ScopedFrameBufferNativeFactory&) = delete;
        ScopedFrameBufferNativeFactory& operator=(const ScopedFrameBufferNativeFactory&) = delete;
        ~ScopedFrameBufferNativeFactory();
    };
    struct FrameBufferBeginScratch {
        vk::RenderPass pass;
        vk::Framebuffer framebuffer;
        vk::Rect2D area;
        std::vector<vk::ClearValue> clears;
        FrameBufferBeginScratch(const impl::FrameBufferImpl& target, std::uint32_t index, vk::Rect2D area, std::span<const ClearValue> values);
    };
    std::vector<ClearValue> default_frame_buffer_clears(const RenderPassDescription& description);
    void validate_frame_buffer_clears(const RenderPassDescription& description, std::span<const ClearValue> values);
    vk::Rect2D frame_buffer_area(const impl::FrameBufferImpl& target, vk::Rect2D area);
    void record_frame_buffer_begin(vk::CommandBuffer buffer, const FrameBufferBeginScratch& scratch);
    void record_frame_buffer_end(vk::CommandBuffer buffer);
    void record_next_subpass(vk::CommandBuffer buffer);
    void initialize_frame_buffer(impl::FrameBufferImpl& target, impl::LogicalDeviceImpl& device, FrameBufferInput input);
    void initialize_frame_buffer(impl::FrameBufferImpl& target, impl::LogicalDeviceImpl& device, FrameBufferInput input, const FrameBufferNativeFactory& factory);
    void destroy_frame_buffer(impl::FrameBufferImpl& target) noexcept;
} // namespace VSL_NAMESPACE::operation
