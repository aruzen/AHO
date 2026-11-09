#include <VSL/vulkan/_pimpls.hpp>

#include <VSL/vulkan/commands/bind_index_buffers.hpp>
#include <VSL/lifetime/identity_access.hpp>
#include <VSL/lifetime/buffer_access.hpp>
#include <stdexcept>

VSL_NAMESPACE::BufferIdentity VSL_NAMESPACE::command::BindIndexBufferAt::identity() const {
    return lifetime::detail::IdentityAccess::From<BufferIdentity>(_source);
}

VSL_NAMESPACE::command::BindIndexBufferAt::BindIndexBufferAt(const BufferView& source, IndexType type, size_t offset, ResourceUse use)
    : _source(lifetime::detail::BufferAccess::Impl(source.ref())),
      _offset(source.view(offset, type == IndexType::UINT16 ? 2 : 4).byte_offset()), _type(type), _use(source.resource_use(std::move(use))) {
    const size_t width = type == IndexType::UINT16 ? 2 : 4;
    if (!_source || !VSL_NAMESPACE::contain(_source->usage, MemoryType::INDEX_BUFFER) ||
        _offset % width || (type != IndexType::UINT16 && type != IndexType::UINT32))
        throw std::invalid_argument("BindIndexBufferAt requires an aligned index buffer range");
}

VSL_NAMESPACE::command::BindIndexBufferAt::BindIndexBufferAt(const BufferAccessor& source, IndexType type, size_t offset, ResourceUse use)
    : BindIndexBufferAt(BufferView::FromBuffer(source), type, offset, std::move(use)) {}

void VSL_NAMESPACE::command::BindIndexBufferAt::invoke(CommandPool pool, CommandBuffer buffer, CommandManager manager) {
    if (_source->device != manager.data_->device.get())
        throw std::invalid_argument("Index buffer and command manager use different devices");
    track(buffer);
    buffer.data_->command_buffers[buffer.get_current_buffer_idx()].bindIndexBuffer(
        _source->buffer, _offset, _type == IndexType::UINT16 ? vk::IndexType::eUint16 : vk::IndexType::eUint32);
}

void VSL_NAMESPACE::command::BindIndexBuffer::invoke(CommandPool pool, CommandBuffer buffer, CommandManager manager) {
    this->buffer->resource_use().record(buffer);
    buffer.data_->command_buffers[buffer.get_current_buffer_idx()]
        .bindIndexBuffer(this->buffer->data_->buffer, offset, vk::IndexType::eUint32);
}
