#include <VSL/lifetime/native_recording.hpp>
#include <VSL/lifetime/command_domain.hpp>
#include <VSL/operation/native_recording.hpp>
#include <VSL/sync/synchronization_state.hpp>
#include <stdexcept>

namespace VSL_NAMESPACE {
    NativeRecordingContext::NativeRecordingContext(CommandBuffer buffer) : _buffer(std::move(buffer)) {
        const auto& domain = lifetime::CommandAccess::Owner(_buffer);
        _context = &domain->context;
        _completion = _buffer.completion();
        if (!_completion || !_context->work)
            throw std::logic_error("Native recording requires an active phase");
        _result = _context->work->result.get();
        _slot = domain->buffer->current_buffer_idx;
        _generation = _context->generation;
        _commands = operation::native_command_buffer(*domain->buffer, _slot);
        require_current();
    }
    void NativeRecordingContext::require_current() const {
        const auto& domain = lifetime::CommandAccess::Owner(_buffer);
        const auto& buffer = *domain->buffer;
        const auto& context = domain->context;
        const auto* work = context.work;
        if (&context != _context || buffer.recording_context != _context || context.generation != _generation ||
            !context.recording || context.closing || context.mutating || context.submitting || context.returning_image || context.frame_buffer_recording ||
            !work || work->result.get() != _result || !sync::recordable(*work->result) || work->frame_slot != _slot ||
            buffer.current_buffer_idx != _slot || operation::native_command_buffer(buffer, _slot) != _commands)
            throw std::logic_error("Native recording context is no longer current");
    }
    void NativeRecordingContext::use(ResourceUse resource, ResourceRetention retention) const {
        const auto context = *this;
        context.require_current();
        resource.record(context._buffer, retention);
        context.require_current();
    }
    void NativeRecordingContext::retain(lifetime::OwnerRef owner, ResourceRetention retention) const {
        const auto context = *this;
        context.require_current();
        if (retention == ResourceRetention::COMPLETION)
            context._buffer.retain_until_complete(owner.share());
        else
            context._completion.retain(owner.share());
        context.require_current();
    }
    VkCommandBuffer NativeRecordingContext::native_command_buffer() const& {
        require_current();
        return _commands;
    }
} // namespace VSL_NAMESPACE
