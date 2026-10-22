#include <VSL/vulkan/commands/draw.hpp>
#include <VSL/vulkan/_pimpls.hpp>
#include <VSL/vulkan/phase.hpp>
#include "draw_count.hpp"

VSL_NAMESPACE::command::DrawManip VSL_NAMESPACE::command::DRAW;

VSL_NAMESPACE::command::Draw::Draw(size_t size) : Draw(size, 1) {}

VSL_NAMESPACE::command::Draw::Draw(size_t size, size_t instance_count, size_t first_vertex, size_t first_instance)
    : _size(impl::helper::checked_draw_count(size)), _instance_count(impl::helper::checked_draw_count(instance_count)),
      _first_vertex(impl::helper::checked_draw_count(first_vertex)), _first_instance(impl::helper::checked_draw_count(first_instance)) {}

void VSL_NAMESPACE::command::Draw::set_size(size_t size) {
    _size = impl::helper::checked_draw_count(size);
}

void VSL_NAMESPACE::command::Draw::invoke(CommandPool pool, CommandBuffer buffer, CommandManager manager) {
    buffer.data_->command_buffers[buffer.get_current_buffer_idx()].draw(_size, _instance_count, _first_vertex, _first_instance);
}

void VSL_NAMESPACE::command::DrawManip::invoke(VSL_NAMESPACE::CommandPool pool, VSL_NAMESPACE::CommandBuffer buffer, VSL_NAMESPACE::CommandManager manager) {
    buffer.data_->command_buffers[buffer.get_current_buffer_idx()].draw(_size.value(), 1, 0, 0);
}

void VSL_NAMESPACE::command::DrawManip::set_vertex_size(std::optional<size_t> vertex_size) {
    if (vertex_size)
        _size = impl::helper::checked_draw_count(*vertex_size);
    else
        _size.reset();
}
