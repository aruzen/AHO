#include <VSL/vulkan/commands/render_pass_begin.hpp>
#include <VSL/vulkan/_pimpls.hpp>
#include "../image_format.hpp"
#include <stdexcept>
#include <type_traits>

namespace VSL_NAMESPACE::command {
    namespace {
        std::vector<ClearValue> defaults(const RenderPassDescription& desc) {
            std::vector<ClearValue> values;
            for (const auto& a : desc.attachments) {
                switch (impl::helper::clear_kind(static_cast<vk::Format>(a.format.type()))) {
                    case 1:
                        values.emplace_back(ClearInt{});
                        break;
                    case 2:
                        values.emplace_back(ClearUint{});
                        break;
                    case 3:
                        values.emplace_back(ClearDepthStencil{});
                        break;
                    default:
                        values.emplace_back(ClearColor{});
                        break;
                }
            }
            return values;
        }
        std::vector<ClearValue> legacy_clears(FrameBufferAccessor target,
                                              std::optional<VSLDRGBColorAccessor<float>> color) {
            if (!color)
                return {};
            if (!target.data_ || !target.data_->render_pass)
                throw std::invalid_argument("Framebuffer required");
            auto values = defaults(RenderPassAccessor{target.data_->render_pass}.description());
            if (values.empty())
                throw std::invalid_argument("Legacy clear color requires an attachment");
            values.front() = ClearColor{color->r, color->g, color->b, 0.f};
            return values;
        }
        vk::ClearValue convert(const ClearValue& value) {
            vk::ClearValue result{};
            std::visit(
                [&](auto v) {
                    using T = decltype(v);
                    if constexpr (std::is_same_v<T, ClearColor>)
                        result.color = vk::ClearColorValue(std::array<float, 4>{v.r, v.g, v.b, v.a});
                    else if constexpr (std::is_same_v<T, ClearInt>)
                        result.color = vk::ClearColorValue(std::array<std::int32_t, 4>{v.r, v.g, v.b, v.a});
                    else if constexpr (std::is_same_v<T, ClearUint>)
                        result.color = vk::ClearColorValue(std::array<std::uint32_t, 4>{v.r, v.g, v.b, v.a});
                    else
                        result.depthStencil = {.depth = v.depth, .stencil = v.stencil};
                },
                value);
            return result;
        }
    } // namespace
    RenderPassBegin::RenderPassBegin(FrameBufferAccessor target, std::vector<ClearValue> clears,
                                     RenderArea render_area, std::uint32_t index)
        : frame_buffer(std::move(target)), clear_values(std::move(clears)), area(render_area), target_index(index) {
        if (!frame_buffer.data_ || !frame_buffer.data_->render_pass)
            throw std::invalid_argument("Framebuffer required");
        render_pass.data_ = frame_buffer.data_->render_pass;
        if (index >= frame_buffer.data_->swap_chain_framebuffers.size())
            throw std::out_of_range("Framebuffer index");
        if (clear_values.empty())
            clear_values = defaults(render_pass.description());
        if (clear_values.size() != render_pass.description().attachments.size())
            throw std::invalid_argument("Clear count must match attachments");
        for (std::size_t i = 0; i < clear_values.size(); ++i) {
            const auto& a = render_pass.description().attachments[i];
            if ((a.load == LoadOp::CLEAR || a.stencil_load == LoadOp::CLEAR) &&
                clear_values[i].index() != impl::helper::clear_kind(static_cast<vk::Format>(a.format.type())))
                throw std::invalid_argument("Clear type does not match attachment format");
        }
        const auto extent = frame_buffer.data_->extent;
        if (!area.width && !area.height) {
            area.width = extent.width;
            area.height = extent.height;
        }
        if (area.x < 0 || area.y < 0 || !area.width || !area.height ||
            static_cast<std::uint64_t>(area.x) + area.width > extent.width ||
            static_cast<std::uint64_t>(area.y) + area.height > extent.height)
            throw std::out_of_range("Render area exceeds framebuffer");
    }
    RenderPassBegin RenderPassBegin::WithClears(FrameBufferAccessor target, std::vector<AttachmentClear> bindings,
                                                RenderArea area, std::uint32_t index) {
        if (!target.data_ || !target.data_->render_pass)
            throw std::invalid_argument("Framebuffer required");
        auto values = defaults(*target.data_->render_pass->description);
        std::vector<bool> bound(values.size());
        for (auto& binding : bindings) {
            if (binding.attachment >= values.size())
                throw std::out_of_range("Clear attachment id");
            if (bound[binding.attachment])
                throw std::invalid_argument("Duplicate clear attachment id");
            bound[binding.attachment] = true;
            values[binding.attachment] = std::move(binding.value);
        }
        return RenderPassBegin(std::move(target), std::move(values), area, index);
    }
    RenderPassBegin::RenderPassBegin(RenderPass pass, FrameBufferAccessor target,
                                     std::optional<VSLDRGBColorAccessor<float>> color)
        : RenderPassBegin(target, legacy_clears(target, color)) {
        if (pass.data_ != render_pass.data_)
            throw std::invalid_argument("RenderPass/Framebuffer mismatch");
        if (color)
            clear_color.emplace(*color);
        target_index.reset();
    }
    IDPickingRenderPassBegin::IDPickingRenderPassBegin(IDPickingRenderPass pass, FrameBufferAccessor target,
                                                       std::optional<VSLDRGBColorAccessor<float>> color)
        : RenderPassBegin(target, legacy_clears(target, color)) {
        if (pass.data_ != render_pass.data_)
            throw std::invalid_argument("RenderPass/Framebuffer mismatch");
        if (color)
            clear_color.emplace(*color);
        target_index.reset();
    }
    void RenderPassBegin::invoke(CommandPool, CommandBuffer buffer, CommandManager) {
        const auto index = target_index.value_or(buffer.data_->acquired_image_index.value_or(buffer.get_current_buffer_idx()));
        buffer.retain(frame_buffer.data_);
        std::vector<vk::ClearValue> values;
        for (const auto& clear : clear_values)
            values.push_back(convert(clear));
        vk::RenderPassBeginInfo info{
            .renderPass = render_pass.data_->render_pass,
            .framebuffer = frame_buffer.data_->swap_chain_framebuffers.at(index),
            .renderArea = {.offset = {area.x, area.y}, .extent = {area.width, area.height}},
            .clearValueCount = static_cast<std::uint32_t>(values.size()),
            .pClearValues = values.data()};
        buffer.data_->command_buffers[buffer.get_current_buffer_idx()].beginRenderPass(info,
                                                                                       vk::SubpassContents::eInline);
    }
    void NextSubpass::invoke(CommandPool, CommandBuffer buffer, CommandManager) {
        buffer.data_->command_buffers[buffer.get_current_buffer_idx()].nextSubpass(vk::SubpassContents::eInline);
    }
} // namespace VSL_NAMESPACE::command
