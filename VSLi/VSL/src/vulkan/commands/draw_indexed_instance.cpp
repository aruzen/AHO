#include <VSL/vulkan/commands/draw_indexed_instance.hpp>
#include <VSL/vulkan/_pimpls.hpp>
#include <VSL/vulkan/phase.hpp>
#include "draw_count.hpp"

VSL_NAMESPACE::command::DrawInstance::DrawInstance(size_t index_size, size_t instance_size)
    : _index_size(impl::helper::checked_draw_count(index_size)), _instance_size(impl::helper::checked_draw_count(instance_size)) {}

VSL_NAMESPACE::command::DrawInstance::DrawInstance(size_t instance_size)
    : DrawInstance(0, instance_size) {}

void VSL_NAMESPACE::command::DrawInstance::set_index_size(size_t size) {
    _index_size = impl::helper::checked_draw_count(size);
}

void VSL_NAMESPACE::command::DrawInstance::set_instance_size(size_t size) {
    _instance_size = impl::helper::checked_draw_count(size);
}

void VSL_NAMESPACE::command::DrawInstance::invoke(CommandPool pool, CommandBuffer buffer, CommandManager manager) {
    buffer.data_->command_buffers[buffer.get_current_buffer_idx()].drawIndexed(_index_size, _instance_size, 0, 0, 0);
}

/*
VSL_NAMESPACE::command::DrawInstanceManip VSL_NAMESPACE::command::drawInstance;

void vsl::command::DrawIndexedManip::set_vertex_size(std::optional<size_t> vertex_size) {
    if (vertex_size)
        size = vertex_size.value();
}
 */
