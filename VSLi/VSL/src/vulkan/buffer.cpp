#include <VSL/lifetime/buffer_storage.hpp>

#include <VSL/vulkan/buffer.hpp>
#include <VSL/vulkan/buffer_and_image_accessor.hpp>
#include <VSL/vulkan/phase.hpp>
#include <VSL/vulkan/commands/copy_image_to_buffer.hpp>

#include <source_location>
#include <stdexcept>
#include <cstring>
#include <utility>

VSL_NAMESPACE::exceptions::MemoryNotHostVisibleException::MemoryNotHostVisibleException(std::string traceinfo) : VSL_NAMESPACE::exceptions::RuntimeException("MemoryNotHostVisibleException",
                                                                                                                                                             "This buffer's memory is not host-visible. Mapping is not allowed.",
                                                                                                                                                             traceinfo) {}

VSL_NAMESPACE::exceptions::MemoryNotHostVisibleException::MemoryNotHostVisibleException(std::source_location sourece) : VSL_NAMESPACE::exceptions::RuntimeException("MemoryNotHostVisibleException",
                                                                                                                                                                    "This buffer's memory is not host-visible. Mapping is not allowed.",
                                                                                                                                                                    sourece) {}

std::uint32_t VSL_NAMESPACE::impl::helper::find_memory_type(std::shared_ptr<VSL_NAMESPACE::impl::LogicalDeviceImpl> device, std::uint32_t type_filter,
                                                            vk::MemoryPropertyFlags properties) {
    if (not device->parent_device->mem_props.has_value())
        device->parent_device->make_mem_props();
    auto& mem_properties = device->parent_device->mem_props.value();
    for (size_t i = 0; i < mem_properties.memoryTypeCount; i++) {
        if ((type_filter & (1 << i)) && (mem_properties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    throw VSL_NAMESPACE::exceptions::RuntimeException("MemoryTypeNotFound", "No compatible memory type satisfies the requested properties");
}

static bool copy_buffer(const std::shared_ptr<VSL_NAMESPACE::impl::BufferImpl>& destination, VSL_NAMESPACE::CommandManager command_manager,
                        VSL_NAMESPACE::BufferAccessor* buf, std::size_t offset = 0, std::optional<std::size_t> size = std::nullopt) {
    const auto source = buf ? buf->data_ : nullptr;
    if (!source || !destination || !source->buffer || !destination->buffer ||
        !source->device || source->device != destination->device || !command_manager.data_ ||
        command_manager.data_->device.get() != destination->device)
        throw std::invalid_argument("Buffer copy requires valid buffers and a command manager on the same device");
    if (!VSL_NAMESPACE::contain(source->usage, VSL_NAMESPACE::MemoryType::TRANSFER_SOURCE) ||
        !VSL_NAMESPACE::contain(destination->usage, VSL_NAMESPACE::MemoryType::TRANSFER_DESTINATION))
        throw std::invalid_argument("Buffer copy requires TransferSource and TransferDestination usage");
    if (source->buffer == destination->buffer)
        throw std::invalid_argument("Buffer copy source and destination must not overlap");
    if (offset > destination->requirement_size)
        throw std::out_of_range("Buffer copy destination offset exceeds storage");
    const auto remaining = destination->requirement_size - offset;
    const auto capacity = size.value_or(remaining);
    if (capacity > remaining || source->requirement_size > capacity)
        throw std::out_of_range("Buffer copy exceeds destination range");

    auto command_buf = command_manager.make_exclusive_buffer();

    auto vk_command_buf = command_buf.data_->command_buffers[0];

    vk::CommandBufferBeginInfo begin_info;
    vk_command_buf.begin(begin_info);

    vk::BufferCopy copy_region{.srcOffset = 0, .dstOffset = offset, .size = source->requirement_size};
    vk_command_buf.copyBuffer(source->buffer, destination->buffer, {copy_region});

    vk_command_buf.end();

    vk::SubmitInfo submit_info;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &vk_command_buf;

    command_manager.data_->graphics_queue.submit({submit_info});
    command_manager.data_->graphics_queue.waitIdle();

    return true;
}

bool VSL_NAMESPACE::BufferAccessor::copy_by_buffer(CommandManager manager, BufferAccessor* source) {
    return copy_buffer(data_, std::move(manager), source);
}

bool VSL_NAMESPACE::BufferAccessor::copy_by_image(CommandManager manager, const VSL_NAMESPACE::ImageAccessor& image) {
    auto phase = manager.start_phase<ComputePhase>();
    phase << command::CopyImageToBuffer(this, image);
    return true;
}

size_t VSL_NAMESPACE::BufferAccessor::size() {
    return data_->requirement_size;
}

std::size_t VSL_NAMESPACE::BufferAccessor::requested_size() const {
    return data_ ? data_->requirement_size : 0;
}

VSL_NAMESPACE::LogicalDeviceAccessor VSL_NAMESPACE::BufferAccessor::get_device() const {
    if (!data_ || !data_->device)
        throw std::logic_error("Buffer is empty");
    // 既存の所有するAccessorへの移行用adapter。通常のbackend操作では呼ばない。
    return LogicalDeviceAccessor{data_->device->shared_from_this()};
}

VSL_NAMESPACE::MemoryType VSL_NAMESPACE::BufferAccessor::get_usage() const {
    return data_ ? data_->usage : MemoryType::NONE;
}

VSL_NAMESPACE::MemoryProperty VSL_NAMESPACE::BufferAccessor::get_memory_properties() const {
    return data_ ? data_->properties : MemoryProperty::NONE;
}

void VSL_NAMESPACE::BufferAccessor::flush_copied(LocalBufferHolder& holder, std::size_t bytes) {
    if (!bytes)
        return;
    holder.size = bytes;
    flush(holder);
}

void VSL_NAMESPACE::BufferAccessor::LocalBufferHolder::flush() {
    flush(size);
}

void VSL_NAMESPACE::BufferAccessor::LocalBufferHolder::synchronize(std::size_t bytes, std::size_t relative_offset, bool flush) const {
    if (!_owner || !data || !_owner->mapped)
        throw std::logic_error("Buffer mapping is empty");
    if (relative_offset > size || bytes > size - relative_offset ||
        offset > _owner->requirement_size || size > _owner->requirement_size - offset)
        throw std::out_of_range("Buffer mapping range");
    if (!bytes || contain(_owner->properties, MemoryProperty::HOST_COHERENT))
        return;
    backend::synchronize_buffer(*_owner, offset + relative_offset, bytes, flush);
}

void VSL_NAMESPACE::BufferAccessor::LocalBufferHolder::flush(std::size_t bytes, std::size_t offset) const {
    synchronize(bytes, offset, true);
}

void VSL_NAMESPACE::BufferAccessor::LocalBufferHolder::invalidate(std::size_t bytes, std::size_t offset) const {
    synchronize(bytes, offset, false);
}

std::span<const std::byte> VSL_NAMESPACE::BufferAccessor::LocalBufferHolder::read(std::size_t bytes, std::size_t offset) const {
    invalidate(bytes, offset);
    return {static_cast<const std::byte*>(data) + offset, bytes};
}

void VSL_NAMESPACE::BufferAccessor::LocalBufferHolder::write(std::span<const std::byte> bytes, std::size_t relative_offset) const {
    if (!_owner || !data)
        throw std::logic_error("Buffer mapping is empty");
    if (relative_offset > size || bytes.size() > size - relative_offset ||
        offset > _owner->requirement_size || size > _owner->requirement_size - offset)
        throw std::out_of_range("Buffer mapping write range");
    if (!bytes.empty())
        std::memmove(static_cast<std::byte*>(data) + relative_offset, bytes.data(), bytes.size());
    flush(bytes.size(), relative_offset);
}

std::shared_ptr<VSL_NAMESPACE::impl::BufferImpl>
VSL_NAMESPACE::BufferAccessor::MakeBuffer(LogicalDeviceAccessor device, size_t size, MemoryType mem_type,
                                          MemoryProperty mem_property, SharingMode sharing_mode) {
    return lifetime::make_heap_buffer(std::move(device), {size, mem_type, mem_property, sharing_mode});
}

void VSL_NAMESPACE::BufferAccessor::FlushBuffer(LocalBufferHolder& holder, MemoryType, MemoryProperty mem_property, SharingMode) {
    if (!contain(mem_property, MemoryProperty::HOST_VISIBLE))
        throw exceptions::MemoryNotHostVisibleException(std::source_location::current());
    holder.flush();
}

VSL_NAMESPACE::BufferAccessor::LocalBufferHolder VSL_NAMESPACE::BufferAccessor::GetData(
    BufferAccessor* buffer, std::optional<size_t> bytes, size_t offset, MemoryType, MemoryProperty mem_property, SharingMode) {
    if (!contain(mem_property, MemoryProperty::HOST_VISIBLE))
        throw exceptions::MemoryNotHostVisibleException(std::source_location::current());
    if (!buffer || !buffer->data_)
        throw std::logic_error("Buffer is empty");
    const auto& storage = buffer->data_;
    if (offset > storage->requirement_size)
        throw exceptions::RuntimeException("buffer", "mapped offset is outside requested storage");
    const auto length = bytes.value_or(storage->requirement_size - offset);
    if (length > storage->requirement_size - offset)
        throw exceptions::RuntimeException("buffer", "mapped range is outside requested storage");
    auto* mapped = backend::map_buffer(*storage, offset, length);
    LocalBufferHolder holder(buffer, length, offset, mapped);
    holder.invalidate(length);
    return holder;
}

bool VSL_NAMESPACE::BufferAccessor::LocalBufferHolder::copy_by_image(CommandManager manager, const ImageAccessor& image) {
    if (!_owner)
        throw std::logic_error("Buffer mapping is empty");
    auto phase = manager.start_phase<ComputePhase>();
    phase << command::CopyImageToBuffer(_owner, image, ImageLayout::GENERAL, {}, true, offset, size);
    return true;
}

bool VSL_NAMESPACE::BufferAccessor::LocalBufferHolder::copy_by_buffer(CommandManager manager, BufferAccessor* buffer) {
    if (!_owner)
        throw std::logic_error("Buffer mapping is empty");
    return copy_buffer(_owner, std::move(manager), buffer, offset, size);
}

VSL_NAMESPACE::BufferAccessor::LocalBufferHolder::~LocalBufferHolder() = default;

VSL_NAMESPACE::BufferAccessor::LocalBufferHolder::LocalBufferHolder(LocalBufferHolder&& other)
    : parent(std::exchange(other.parent, nullptr)), size(std::exchange(other.size, 0)), offset(std::exchange(other.offset, 0)),
      data(std::exchange(other.data, nullptr)), _owner(std::move(other._owner)) {}

VSL_NAMESPACE::BufferAccessor::LocalBufferHolder::LocalBufferHolder(BufferAccessor* parent, std::size_t size, std::size_t offset, void* data)
    : parent(parent), size(size), offset(offset), data(data), _owner(parent ? parent->data_ : nullptr) {}

bool VSL_NAMESPACE::helper::copy_with_shift_offset(void* data, size_t max_size, size_t& offset) {
    return true;
}
