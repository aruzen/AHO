#include <VSL/vulkan/commands/draw_indexed.hpp>
#include <VSL/vulkan/_pimpls.hpp>
#include <VSL/vulkan/phase.hpp>
#include "draw_count.hpp"

VSL_NAMESPACE::command::DrawIndexedManip VSL_NAMESPACE::command::DRAW_INDEXED;

VSL_NAMESPACE::command::DrawIndexed::DrawIndexed(size_t size) : _size(impl::helper::checked_draw_count(size)) {}

void VSL_NAMESPACE::command::DrawIndexed::set_size(size_t size) {
    _size = impl::helper::checked_draw_count(size);
}

void VSL_NAMESPACE::command::DrawIndexed::invoke(CommandPool pool, CommandBuffer buffer, CommandManager manager) {
    buffer.data_->command_buffers[buffer.get_current_buffer_idx()].drawIndexed(_size, 1, 0, 0, 0);
}

void VSL_NAMESPACE::command::DrawIndexedManip::set_vertex_size(std::optional<size_t> vertex_size) {
    if (vertex_size)
        set_size(*vertex_size);
}
