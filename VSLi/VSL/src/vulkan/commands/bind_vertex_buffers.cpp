#include <VSL/lifetime/command_access.hpp>
#include <VSL/vulkan/_pimpls.hpp>

#include <VSL/vulkan/commands/bind_vertex_buffers.hpp>
#include <VSL/lifetime/identity_access.hpp>
#include <VSL/lifetime/buffer_access.hpp>

#include <ranges>
#include <stdexcept>

namespace {
    using VSL_NAMESPACE::lifetime::CommandAccess;
}
VSL_NAMESPACE::BufferIdentity VSL_NAMESPACE::command::BindVertexBufferAt::identity() const {
    return lifetime::detail::IdentityAccess::From<BufferIdentity>(_source);
}

VSL_NAMESPACE::command::BindVertexBufferAt::BindVertexBufferAt(std::uint32_t binding, const BufferAccessor& source, size_t offset, ResourceUse use)
    : BindVertexBufferAt(binding, BufferView::FromBuffer(source), offset, std::move(use)) {}

VSL_NAMESPACE::command::BindVertexBufferAt::BindVertexBufferAt(std::uint32_t binding, const BufferView& source, size_t offset, ResourceUse use)
    : _binding(binding), _offset(source.view(offset, 1).byte_offset()),
      _source(lifetime::detail::BufferAccess::Impl(source.ref())), _use(source.resource_use(std::move(use))) {
    if (_binding == UINT32_MAX || !_source || !VSL_NAMESPACE::contain(_source->usage, MemoryType::VERTEX_BUFFER))
        throw std::invalid_argument("BindVertexBufferAt requires a vertex buffer");
}

void VSL_NAMESPACE::command::BindVertexBufferAt::invoke(CommandPool pool, CommandBuffer buffer, CommandManager manager) {
    if (_source->device != CommandAccess::Data(manager)->device.get())
        throw std::invalid_argument("Vertex buffer and command manager use different devices");
    track(buffer);
    CommandAccess::Data(buffer)->command_buffers[buffer.get_current_buffer_idx()].bindVertexBuffers(_binding, {_source->buffer}, {_offset});
}

void VSL_NAMESPACE::command::BindVertexBuffer::invoke(CommandPool pool, CommandBuffer buffer, CommandManager manager) {
    if (buffers.empty())
        return;
    for (auto* source : buffers)
        source->resource_use().record(buffer);
    CommandAccess::Data(buffer)->command_buffers[buffer.get_current_buffer_idx()].bindVertexBuffers(0,
                                                                                                    this->buffers | std::views::transform([](auto& b) { return b->data_->buffer; }) | std::ranges::to<std::vector<vk::Buffer>>(),
                                                                                                    std::vector<vk::DeviceSize>(this->buffers.size(), 0));
}
