#include <VSL/vulkan/commands/draw_indexed.hpp>
#include <VSL/vulkan/_pimpls.hpp>
#include <VSL/vulkan/phase.hpp>
#include "draw_count.hpp"

VSL_NAMESPACE::command::DrawIndexedManip VSL_NAMESPACE::command::DRAW_INDEXED;

VSL_NAMESPACE::command::DrawIndexed::DrawIndexed(size_t size) : DrawIndexed(size, 1) {}

VSL_NAMESPACE::command::DrawIndexed::DrawIndexed(size_t size, size_t instance_count, size_t first_index, std::int64_t vertex_offset, size_t first_instance)
    : _size(impl::helper::checked_draw_count(size)), _instance_count(impl::helper::checked_draw_count(instance_count)),
      _first_index(impl::helper::checked_draw_count(first_index)), _first_instance(impl::helper::checked_draw_count(first_instance)) {
    if (!std::in_range<std::int32_t>(vertex_offset))
        throw exceptions::RuntimeException("DrawIndexed", "Vertex offset exceeds int32_t range");
    _vertex_offset = static_cast<std::int32_t>(vertex_offset);
}

void VSL_NAMESPACE::command::DrawIndexed::set_size(size_t size) {
    _size = impl::helper::checked_draw_count(size);
}

void VSL_NAMESPACE::command::DrawIndexed::invoke(CommandPool pool, CommandBuffer buffer, CommandManager manager) {
    buffer.data_->command_buffers[buffer.get_current_buffer_idx()].drawIndexed(_size, _instance_count, _first_index, _vertex_offset, _first_instance);
}

void VSL_NAMESPACE::command::DrawIndexedManip::set_vertex_size(std::optional<size_t> vertex_size) {
    if (vertex_size)
        set_size(*vertex_size);
}
