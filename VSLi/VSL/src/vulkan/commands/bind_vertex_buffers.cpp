#include <VSL/vulkan/_pimpls.hpp>

#include <VSL/vulkan/commands/bind_vertex_buffers.hpp>

#include <ranges>

void VSL_NAMESPACE::command::BindVertexBuffer::invoke(CommandPool pool, CommandBuffer buffer, CommandManager manager) {
    for (auto* source : buffers)
        buffer.retain(source->data_);
    buffer.data_->command_buffers[buffer.get_current_buffer_idx()]
        .bindVertexBuffers(0,
                           this->buffers | std::views::transform([](auto& b) { return b->data_->buffer; }) | std::ranges::to<std::vector<vk::Buffer>>(),
                           std::vector<vk::DeviceSize>(this->buffers.size(), 0));
}
