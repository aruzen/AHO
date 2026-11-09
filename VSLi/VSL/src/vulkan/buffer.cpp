#include <VSL/lifetime/command_access.hpp>
#include <VSL/lifetime/device_owner.hpp>
#include <VSL/lifetime/resource_use_factory.hpp>
#include <VSL/lifetime/identity_access.hpp>
#include <VSL/lifetime/buffer_storage.hpp>
#include <VSL/lifetime/buffer_access.hpp>
#include <VSL/lifetime/owner_access.hpp>

#include <VSL/vulkan/buffer.hpp>
#include <VSL/lifetime/buffer_view.hpp>
#include <VSL/vulkan/buffer_and_image_accessor.hpp>
#include <VSL/vulkan/phase.hpp>
#include <VSL/vulkan/commands/copy_image_to_buffer.hpp>

#include <source_location>
#include <stdexcept>
#include <cstring>
#include <utility>

namespace {
    using VSL_NAMESPACE::lifetime::detail::BufferAccess;
}

namespace {
    using VSL_NAMESPACE::lifetime::CommandAccess;
}
VSL_NAMESPACE::exceptions::MemoryNotHostVisibleException::MemoryNotHostVisibleException(std::string traceinfo) : VSL_NAMESPACE::exceptions::RuntimeException("MemoryNotHostVisibleException",
                                                                                                                                                             "This buffer's memory is not host-visible. Mapping is not allowed.",
                                                                                                                                                             traceinfo) {}

VSL_NAMESPACE::exceptions::MemoryNotHostVisibleException::MemoryNotHostVisibleException(std::source_location sourece) : VSL_NAMESPACE::exceptions::RuntimeException("MemoryNotHostVisibleException",
                                                                                                                                                                    "This buffer's memory is not host-visible. Mapping is not allowed.",
                                                                                                                                                                    sourece) {}

VSL_NAMESPACE::BufferIdentity VSL_NAMESPACE::BufferAccessor::identity() const {
    return lifetime::detail::IdentityAccess::From<BufferIdentity>(_owner.get());
}

void VSL_NAMESPACE::lifetime::detail::BufferAccess::SetShared(BufferAccessor& buffer, std::shared_ptr<impl::BufferImpl> owner) {
    const auto* node = owner ? owner->owner_node : nullptr;
    buffer._owner = owned_ref(std::move(owner), node);
}

VSL_NAMESPACE::BufferAccessor::BufferAccessor(LogicalDeviceAccessor device, const BufferCreateInfo& info) {
    lifetime::detail::BufferAccess::SetShared(*this, lifetime::make_heap_buffer(std::move(device), info));
}

static bool copy_buffer(VSL_NAMESPACE::lifetime::OwnedRef<VSL_NAMESPACE::impl::BufferImpl> destination, VSL_NAMESPACE::CommandManager command_manager,
                        VSL_NAMESPACE::BufferAccessor* buf, std::size_t offset = 0, std::optional<std::size_t> size = std::nullopt) {
    const auto source = buf ? BufferAccess::Owned(*(buf)) : nullptr;
    if (!source || !destination || !source->buffer || !destination->buffer ||
        !source->device || source->device != destination->device || !CommandAccess::Data(command_manager) ||
        CommandAccess::Data(command_manager)->device.get() != destination->device)
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
    source->use->require_idle();
    destination->use->require_idle();

    auto command_buf = command_manager.make_exclusive_buffer();

    auto vk_command_buf = CommandAccess::Data(command_buf)->command_buffers[0];

    vk::CommandBufferBeginInfo begin_info;
    vk_command_buf.begin(begin_info);

    vk::BufferCopy copy_region{.srcOffset = 0, .dstOffset = offset, .size = source->requirement_size};
    vk_command_buf.copyBuffer(source->buffer, destination->buffer, {copy_region});

    vk_command_buf.end();

    vk::SubmitInfo submit_info;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &vk_command_buf;

    CommandAccess::Data(command_manager)->graphics_queue.submit({submit_info});
    CommandAccess::Data(command_manager)->graphics_queue.waitIdle();

    return true;
}

bool VSL_NAMESPACE::BufferAccessor::copy_by_buffer(CommandManager manager, BufferAccessor* source) {
    return copy_buffer(_owner, std::move(manager), source);
}

bool VSL_NAMESPACE::BufferAccessor::copy_by_image(CommandManager manager, const VSL_NAMESPACE::ImageAccessor& image) {
    const auto copy = command::CopyImageToBuffer(BufferView::FromBuffer(*this), image, ImageLayout::GENERAL, {}, false, {}, ResourceRetention::SUBMISSION);
    auto phase = manager.start_phase<ComputePhase>();
    phase << copy;
    return true;
}

size_t VSL_NAMESPACE::BufferAccessor::size() {
    if (!_owner)
        throw std::logic_error("Buffer is empty");
    return _owner->requirement_size;
}

std::size_t VSL_NAMESPACE::BufferAccessor::requested_size() const {
    return _owner ? _owner->requirement_size : 0;
}

VSL_NAMESPACE::LogicalDeviceAccessor VSL_NAMESPACE::BufferAccessor::get_device() const {
    if (!_owner || !_owner->device || !_owner->buffer)
        throw std::logic_error("Buffer is empty");
    // 既存の所有するAccessorへの移行用adapter。通常のbackend操作では呼ばない。
    return lifetime::device_from_child(_owner);
}

VSL_NAMESPACE::MemoryType VSL_NAMESPACE::BufferAccessor::get_usage() const {
    return _owner ? _owner->usage : MemoryType::NONE;
}

VSL_NAMESPACE::MemoryProperty VSL_NAMESPACE::BufferAccessor::get_memory_properties() const {
    return _owner ? _owner->properties : MemoryProperty::NONE;
}

VSL_NAMESPACE::ResourceUseState& VSL_NAMESPACE::BufferAccessor::use_state() const {
    return lifetime::detail::BufferAccess::Ref(_owner.get()).use();
}
VSL_NAMESPACE::ResourceUse VSL_NAMESPACE::BufferAccessor::resource_use() const {
    return lifetime::resource_use_for_buffer(_owner, {});
}
VSL_NAMESPACE::ResourceUse VSL_NAMESPACE::BufferAccessor::resource_use(ResourceUse constraint) const {
    return lifetime::resource_use_for_buffer(_owner, std::move(constraint));
}
void VSL_NAMESPACE::BufferAccessor::read_into(std::span<std::byte> destination, size_t offset) const {
    const auto owner = _owner;
    lifetime::detail::BufferAccess::Ref(owner.get()).read_into(destination, offset);
}

void VSL_NAMESPACE::BufferAccessor::flush_copied(LocalBufferHolder& holder, std::size_t bytes) {
    if (!bytes)
        return;
    holder.size = bytes;
    flush(holder);
}

void VSL_NAMESPACE::BufferAccessor::LocalBufferHolder::flush() {
    const auto mapping = snapshot_idle();
    Synchronize(mapping, mapping.size, 0, true);
}

VSL_NAMESPACE::BufferAccessor::LocalBufferHolder::MappingSnapshot VSL_NAMESPACE::BufferAccessor::LocalBufferHolder::snapshot_idle() const {
    MappingSnapshot mapping{_owner, data, size, offset};
    // 互換用にCPU領域だけを渡したholderには、GPU storageの状態はない。
    if (mapping.owner)
        mapping.owner->use->require_idle();
    return mapping;
}

void VSL_NAMESPACE::BufferAccessor::LocalBufferHolder::ValidateRange(const MappingSnapshot& mapping, std::size_t bytes, std::size_t relative_offset) {
    if (!mapping.owner || !mapping.data || !mapping.owner->mapped)
        throw std::logic_error("Buffer mapping is empty");
    if (relative_offset > mapping.size || bytes > mapping.size - relative_offset ||
        mapping.offset > mapping.owner->requirement_size || mapping.size > mapping.owner->requirement_size - mapping.offset)
        throw std::out_of_range("Buffer mapping range");
}

void VSL_NAMESPACE::BufferAccessor::LocalBufferHolder::Synchronize(const MappingSnapshot& mapping, std::size_t bytes, std::size_t relative_offset, bool flush) {
    ValidateRange(mapping, bytes, relative_offset);
    if (!bytes || contain(mapping.owner->properties, MemoryProperty::HOST_COHERENT))
        return;
    operation::synchronize_buffer(*mapping.owner, mapping.offset + relative_offset, bytes, flush);
}

void VSL_NAMESPACE::BufferAccessor::LocalBufferHolder::flush(std::size_t bytes, std::size_t offset) const {
    const auto mapping = snapshot_idle();
    Synchronize(mapping, bytes, offset, true);
}

void VSL_NAMESPACE::BufferAccessor::LocalBufferHolder::invalidate(std::size_t bytes, std::size_t offset) const {
    const auto mapping = snapshot_idle();
    Synchronize(mapping, bytes, offset, false);
}

std::span<const std::byte> VSL_NAMESPACE::BufferAccessor::LocalBufferHolder::read(std::size_t bytes, std::size_t offset) const {
    const auto mapping = snapshot_idle();
    Synchronize(mapping, bytes, offset, false);
    return {static_cast<const std::byte*>(mapping.data) + offset, bytes};
}

void VSL_NAMESPACE::BufferAccessor::LocalBufferHolder::write(std::span<const std::byte> bytes, std::size_t relative_offset) const {
    const auto mapping = snapshot_idle();
    ValidateRange(mapping, bytes.size(), relative_offset);
    if (!bytes.empty())
        std::memmove(static_cast<std::byte*>(mapping.data) + relative_offset, bytes.data(), bytes.size());
    Synchronize(mapping, bytes.size(), relative_offset, true);
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
    if (!buffer || !buffer->_owner)
        throw std::logic_error("Buffer is empty");
    const auto storage = buffer->_owner;
    storage->use->require_idle();
    if (offset > storage->requirement_size)
        throw exceptions::RuntimeException("buffer", "mapped offset is outside requested storage");
    const auto length = bytes.value_or(storage->requirement_size - offset);
    if (length > storage->requirement_size - offset)
        throw exceptions::RuntimeException("buffer", "mapped range is outside requested storage");
    auto* mapped = operation::map_buffer(*storage, offset, length);
    LocalBufferHolder holder(buffer, length, offset, mapped, storage);
    LocalBufferHolder::Synchronize({storage, mapped, length, offset}, length, 0, false);
    return holder;
}

bool VSL_NAMESPACE::BufferAccessor::LocalBufferHolder::copy_by_image(CommandManager manager, const ImageAccessor& image) {
    const auto owner = _owner;
    if (!owner)
        throw std::logic_error("Buffer mapping is empty");
    const auto destination = BufferAccess::View(BufferAccess::Ref(owner.get()), owner, offset, size);
    const auto copy = command::CopyImageToBuffer(destination, image, ImageLayout::GENERAL, {}, true, {}, ResourceRetention::SUBMISSION);
    auto phase = manager.start_phase<ComputePhase>();
    phase << copy;
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
    : parent(parent), size(size), offset(offset), data(data), _owner(parent ? BufferAccess::Owned(*(parent)) : nullptr) {}

VSL_NAMESPACE::BufferAccessor::LocalBufferHolder::LocalBufferHolder(BufferAccessor* parent, std::size_t size, std::size_t offset, void* data,
                                                                    lifetime::OwnedRef<impl::BufferImpl> owner)
    : parent(parent), size(size), offset(offset), data(data), _owner(std::move(owner)) {}

bool VSL_NAMESPACE::helper::copy_with_shift_offset(void* data, size_t max_size, size_t& offset) {
    return true;
}
