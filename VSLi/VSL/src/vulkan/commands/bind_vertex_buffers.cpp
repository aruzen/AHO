#include <VSL/vulkan/_pimpls.hpp>

#include <VSL/vulkan/commands/bind_vertex_buffers.hpp>

#include <ranges>
#include <stdexcept>

VSL_NAMESPACE::command::BindVertexBufferAt::BindVertexBufferAt(std::uint32_t binding, const BufferAccessor& source)
    : _binding(binding), _source(source.data_) {
    if (_binding == UINT32_MAX || !_source || !VSL_NAMESPACE::contain(_source->usage, MemoryType::VERTEX_BUFFER))
        throw std::invalid_argument("BindVertexBufferAt requires a vertex buffer");
}

void VSL_NAMESPACE::command::BindVertexBufferAt::invoke(CommandPool pool, CommandBuffer buffer, CommandManager manager) {
    if (_source->device != manager.data_->device)
        throw std::invalid_argument("Vertex buffer and command manager use different devices");
    buffer.retain(_source);
    buffer.data_->command_buffers[buffer.get_current_buffer_idx()].bindVertexBuffers(_binding, {_source->buffer}, {0});
}

void VSL_NAMESPACE::command::BindVertexBuffer::invoke(CommandPool pool, CommandBuffer buffer, CommandManager manager) {
    if (buffers.empty())
        return;
    for (auto* source : buffers)
        buffer.retain(source->data_);
    buffer.data_->command_buffers[buffer.get_current_buffer_idx()]
        .bindVertexBuffers(0,
                           this->buffers | std::views::transform([](auto& b) { return b->data_->buffer; }) | std::ranges::to<std::vector<vk::Buffer>>(),
                           std::vector<vk::DeviceSize>(this->buffers.size(), 0));
}
