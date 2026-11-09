#include <VSL/lifetime/command_domain.hpp>
#include <VSL/vulkan/commands/render_pass_begin.hpp>
#include <VSL/vulkan/_pimpls.hpp>
#include <VSL/sync/resource_use_state.hpp>
#include <VSL/lifetime/submission_retention.hpp>
#include <VSL/operation/frame_buffer_ops.hpp>
#include <stdexcept>

namespace VSL_NAMESPACE::command {
    namespace {
        struct FrameBufferRecording {
            impl::CommandBufferImpl& buffer;
            explicit FrameBufferRecording(impl::CommandBufferImpl& buffer);
            ~FrameBufferRecording() { buffer.recording_context->frame_buffer_recording = false; }
        };
        FrameBufferRecording::FrameBufferRecording(impl::CommandBufferImpl& buffer) : buffer(buffer) {
            if (buffer.recording_context->frame_buffer_recording)
                throw std::logic_error("Framebuffer recording cannot reenter the same command buffer");
            buffer.recording_context->frame_buffer_recording = true;
        }
        std::vector<ClearValue> legacy_clears(FrameBufferAccessor target,
                                              std::optional<VSLDRGBColorAccessor<float>> color) {
            if (!color)
                return {};
            const auto pass = target.get_render_pass();
            auto values = operation::default_frame_buffer_clears(pass.description());
            if (values.empty())
                throw std::invalid_argument("Legacy clear color requires an attachment");
            values.front() = ClearColor{color->r, color->g, color->b, 0.f};
            return values;
        }
    } // namespace
    RenderPassBegin::RenderPassBegin(FrameBufferAccessor target, std::vector<ClearValue> clears,
                                     RenderArea render_area, std::uint32_t index, ResourceRetention retention)
        : frame_buffer(std::move(target)), clear_values(std::move(clears)), area(render_area), target_index(index), retention(retention) {
        if (retention != ResourceRetention::SUBMISSION && retention != ResourceRetention::COMPLETION)
            throw std::invalid_argument("Invalid framebuffer retention policy");
        render_pass = frame_buffer.get_render_pass();
        if (index >= frame_buffer.data_->swap_chain_framebuffers.size())
            throw std::out_of_range("Framebuffer index");
        if (clear_values.empty())
            clear_values = operation::default_frame_buffer_clears(render_pass.description());
        operation::validate_frame_buffer_clears(render_pass.description(), clear_values);
        const auto checked = operation::frame_buffer_area(*frame_buffer.data_, {{area.x, area.y}, {area.width, area.height}});
        area = {checked.offset.x, checked.offset.y, checked.extent.width, checked.extent.height};
    }
    RenderPassBegin RenderPassBegin::WithClears(FrameBufferAccessor target, std::vector<AttachmentClear> bindings,
                                                RenderArea area, std::uint32_t index, ResourceRetention retention) {
        const auto pass = target.get_render_pass();
        auto values = operation::default_frame_buffer_clears(pass.description());
        std::vector<bool> bound(values.size());
        for (auto& binding : bindings) {
            if (binding.attachment >= values.size())
                throw std::out_of_range("Clear attachment id");
            if (bound[binding.attachment])
                throw std::invalid_argument("Duplicate clear attachment id");
            bound[binding.attachment] = true;
            values[binding.attachment] = std::move(binding.value);
        }
        return RenderPassBegin(std::move(target), std::move(values), area, index, retention);
    }
    RenderPassBegin::RenderPassBegin(RenderPass pass, FrameBufferAccessor target,
                                     std::optional<VSLDRGBColorAccessor<float>> color, ResourceRetention retention)
        : RenderPassBegin(target, legacy_clears(target, color), {}, 0, retention) {
        if (pass.data_ != render_pass.data_)
            throw std::invalid_argument("RenderPass/Framebuffer mismatch");
        if (color)
            clear_color.emplace(*color);
        target_index.reset();
    }
    IDPickingRenderPassBegin::IDPickingRenderPassBegin(IDPickingRenderPass pass, FrameBufferAccessor target,
                                                       std::optional<VSLDRGBColorAccessor<float>> color, ResourceRetention retention)
        : RenderPassBegin(target, legacy_clears(target, color), {}, 0, retention) {
        if (pass.data_ != render_pass.data_)
            throw std::invalid_argument("RenderPass/Framebuffer mismatch");
        if (color)
            clear_color.emplace(*color);
        target_index.reset();
    }
    void RenderPassBegin::invoke(CommandPool, CommandBuffer buffer, CommandManager) {
        // poll中のcallbackがcommandや元wrapperを破棄しても、このsnapshotだけを使う。
        const auto target = frame_buffer.data_;
        const auto policy = retention;
        if (policy != ResourceRetention::SUBMISSION && policy != ResourceRetention::COMPLETION)
            throw std::invalid_argument("Invalid framebuffer retention policy");
        if (!target || !target.use_count() || !target->device || !target->render_pass || !target->render_pass->description ||
            !buffer.data_ || !buffer.data_->command_pool || buffer.data_->command_pool->device != target->device)
            throw std::invalid_argument("Render pass begin requires a target on the recording device");
        const FrameBufferRecording recording(*buffer.data_);
        const auto completion = buffer.completion();
        lifetime::require_recording(buffer, completion);
        const auto slot = buffer.get_current_buffer_idx();
        const auto native = buffer.data_->command_buffers.at(slot);
        const auto index = target_index.value_or(buffer.data_->recording_context->acquired_image_index.value_or(slot));
        const operation::FrameBufferBeginScratch scratch(*target, index, {{area.x, area.y}, {area.width, area.height}}, clear_values);
        if (policy == ResourceRetention::COMPLETION)
            buffer.retain_until_complete(target);
        else
            buffer.retain(target);
        for (const auto* image : target->attachments.at(index))
            image->use->mark_used(completion);
        lifetime::require_recording(buffer, completion);
        if (buffer.get_current_buffer_idx() != slot || buffer.data_->command_buffers.at(slot) != native)
            throw std::logic_error("Recording buffer changed while tracking framebuffer images");
        operation::record_frame_buffer_begin(native, scratch);
    }
    void NextSubpass::invoke(CommandPool, CommandBuffer buffer, CommandManager) {
        if (buffer.data_->recording_context->frame_buffer_recording)
            throw std::logic_error("Subpass recording cannot reenter framebuffer tracking");
        operation::record_next_subpass(buffer.data_->command_buffers.at(buffer.get_current_buffer_idx()));
    }
} // namespace VSL_NAMESPACE::command
