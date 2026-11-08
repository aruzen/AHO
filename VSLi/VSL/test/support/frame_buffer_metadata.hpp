#pragma once
#include <VSL/lifetime/frame_buffer_storage.hpp>

namespace vsl_test {
    // Native記録を行わないclear/area検証用。実体と元Passの所有を明示する。
    struct FrameBufferMetadata {
        std::shared_ptr<vsl::impl::RenderPassImpl> pass;
        vsl::lifetime::FrameBufferOwnerContext context;
        vsl::impl::FrameBufferImpl target;
        FrameBufferMetadata(std::shared_ptr<vsl::impl::RenderPassImpl> pass, std::uint32_t width, std::uint32_t height);
    };
    inline vsl::FrameBufferAccessor metadata_frame_buffer(std::shared_ptr<vsl::impl::RenderPassImpl> pass, std::uint32_t width = 1, std::uint32_t height = 1);
    inline FrameBufferMetadata::FrameBufferMetadata(std::shared_ptr<vsl::impl::RenderPassImpl> pass, std::uint32_t width, std::uint32_t height)
        : pass(std::move(pass)) {
        target.render_pass = this->pass.get();
        target.owner_context = &context;
        target.swap_chain_framebuffers.emplace_back();
        target.extent = vk::Extent2D{width, height};
        const vsl::RenderPassAccessor source{this->pass};
        context.pass = source.owner_ref().alias(this->pass.get()).weak();
    }
    inline vsl::FrameBufferAccessor metadata_frame_buffer(std::shared_ptr<vsl::impl::RenderPassImpl> pass, std::uint32_t width, std::uint32_t height) {
        auto owner = std::make_shared<FrameBufferMetadata>(std::move(pass), width, height);
        auto* value = &owner->target;
        return {{std::move(owner), value}};
    }
} // namespace vsl_test
