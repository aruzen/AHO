#include <VSL/lifetime/command_access.hpp>
#include <VSL/lifetime/device_owner.hpp>
#include <VSL/define.hpp>

#include <VSL/vulkan/_pimpls.hpp>
#include <VSL/exceptions.hpp>

#include <VSL/vulkan/vulkan.hpp>
#include <VSL/vulkan/command.hpp>
#include <VSL/vulkan/phase.hpp>
#include <VSL/vulkan/surface.hpp>
#include <VSL/lifetime/command_domain.hpp>
#include <VSL/lifetime/submission_retention.hpp>
#include <VSL/lifetime/command_storage.hpp>
#include <VSL/lifetime/owner_access.hpp>
#include <VSL/lifetime/identity_access.hpp>
#include <limits>
#include <stdexcept>

namespace {
    using VSL_NAMESPACE::lifetime::CommandAccess;
}
VSL_NAMESPACE::CommandPool::CommandPool(VSL_NAMESPACE::LogicalDeviceAccessor device, CommandPoolCreateInfo info)
    : _data(lifetime::make_heap_command_pool(device, info)) {}

VSL_NAMESPACE::CommandPoolIdentity VSL_NAMESPACE::CommandPool::identity() const {
    return lifetime::detail::IdentityAccess::From<CommandPoolIdentity>(_data.get());
}
VSL_NAMESPACE::CommandBufferIdentity VSL_NAMESPACE::CommandBuffer::identity() const {
    return lifetime::detail::IdentityAccess::From<CommandBufferIdentity>(_data.get());
}
VSL_NAMESPACE::CommandManagerIdentity VSL_NAMESPACE::CommandManager::identity() const {
    return lifetime::detail::IdentityAccess::From<CommandManagerIdentity>(_data.get());
}

VSL_NAMESPACE::LogicalDeviceAccessor VSL_NAMESPACE::CommandPool::get_device() const {
    (void)owner_ref();
    if (!_data->device || !_data->native_device || !_data->command_pool)
        throw std::invalid_argument("Command pool is empty");
    return lifetime::device_from_child(_data);
}
VSL_NAMESPACE::lifetime::OwnerRef VSL_NAMESPACE::CommandPool::owner_ref() const {
    if (!_data || !_data.use_count() || !_data->owner_node || !_data->owner_node->matches(_data))
        throw std::invalid_argument("Command pool requires its canonical owner");
    return lifetime::detail::owned_ref(_data, _data->owner_node);
}

VSL_NAMESPACE::CommandPool::CommandPool(std::shared_ptr<VSL_NAMESPACE::impl::CommandPoolImpl> data) : _data(data) {}

VSL_NAMESPACE::CommandBuffer::CommandBuffer(VSL_NAMESPACE::CommandPool pool, std::uint32_t size) {
    *this = lifetime::CommandAccess::Attach(lifetime::make_heap_command_buffer(pool, size));
}

VSL_NAMESPACE::CommandBuffer::CommandBuffer(std::shared_ptr<VSL_NAMESPACE::impl::CommandBufferImpl> data)
    : _data(std::move(data)), _domain(_data ? lifetime::CommandAccess::Resolve(_data) : nullptr) {}

VSL_NAMESPACE::CommandBuffer& VSL_NAMESPACE::CommandBuffer::operator=(CommandBuffer other) noexcept {
    swap(other);
    return *this;
}
void VSL_NAMESPACE::CommandBuffer::swap(CommandBuffer& other) noexcept {
    _data.swap(other._data);
    _domain.swap(other._domain);
}
VSL_NAMESPACE::CommandPool VSL_NAMESPACE::CommandBuffer::get_pool() const {
    (void)owner_ref();
    if (!_data->owner_context || !_data->command_pool)
        throw std::invalid_argument("Command ring is empty");
    auto pool = _data->owner_context->pool.lock();
    if (!pool.use_count() || pool.get() != _data->command_pool)
        throw std::logic_error("Command ring pool owner has expired");
    return {CommandAccess::Pool(pool.share())};
}
VSL_NAMESPACE::lifetime::OwnerRef VSL_NAMESPACE::CommandBuffer::owner_ref() const {
    if (!_data || !_data.use_count() || !_data->owner_node || !_data->owner_node->matches(_data))
        throw std::invalid_argument("Command ring requires its canonical owner");
    return lifetime::detail::owned_ref(_data, _data->owner_node);
}

std::uint32_t VSL_NAMESPACE::CommandBuffer::get_current_buffer_idx() {
    if (!_data)
        throw std::invalid_argument("Command ring is empty");
    return _data->current_buffer_idx;
}

size_t VSL_NAMESPACE::CommandBuffer::get_size() {
    if (!_data)
        throw std::invalid_argument("Command ring is empty");
    return _data->command_buffers.size();
}

void VSL_NAMESPACE::CommandBuffer::next() {
    lifetime::next_command_buffer(*this);
}

void VSL_NAMESPACE::CommandBuffer::reset() {
    lifetime::reset_command_buffers(*this, std::nullopt);
}
void VSL_NAMESPACE::CommandBuffer::require_recording() const {
    lifetime::require_recording(*this, completion());
}
void VSL_NAMESPACE::CommandBuffer::require_recording(const Completion& expected) const {
    if (!expected) {
        const auto* context = _data ? _data->recording_context : nullptr;
        if (!context || context->closing || context->recording || context->submitting || context->mutating ||
            context->returning_image || context->frame_buffer_recording || context->work)
            throw std::logic_error("Idle stream context changed while resolving a command");
        return;
    }
    lifetime::require_recording(*this, expected);
}

void VSL_NAMESPACE::CommandBuffer::reset(std::uint32_t idx) {
    lifetime::reset_command_buffers(*this, idx);
}

VSL_NAMESPACE::CommandManager::CommandManager(std::shared_ptr<VSL_NAMESPACE::impl::CommandManagerImpl> data)
    : _data(std::move(data)), _domain(_data ? lifetime::CommandAccess::Resolve(_data->command_buffer) : nullptr) {}

VSL_NAMESPACE::CommandManager& VSL_NAMESPACE::CommandManager::operator=(CommandManager other) noexcept {
    swap(other);
    return *this;
}
void VSL_NAMESPACE::CommandManager::swap(CommandManager& other) noexcept {
    _data.swap(other._data);
    _domain.swap(other._domain);
}

VSL_NAMESPACE::CommandManager::CommandManager(VSL_NAMESPACE::LogicalDeviceAccessor device) {
    *this = FromBuffer(CommandBuffer(CommandPool(device)));
}
VSL_NAMESPACE::CommandManager VSL_NAMESPACE::CommandManager::FromBuffer(CommandBuffer buffer) {
    const auto domain = lifetime::CommandAccess::Owner(buffer);
    const auto pool = buffer.get_pool();
    const auto device = pool.get_device();
    auto data = std::make_shared<impl::CommandManagerImpl>();
    data->device = device.data_;
    data->command_pool = CommandAccess::Data(pool);
    data->command_buffer = CommandAccess::Data(buffer);
    data->graphics_queue = device.data_->device.getQueue(device.data_->graphics_family, 0);
    data->present_queue = device.data_->device.getQueue(device.data_->present_family, 0);
    return lifetime::CommandAccess::Manager(std::move(data), domain);
}

VSL_NAMESPACE::CommandPool VSL_NAMESPACE::CommandManager::get_pool() {
    if (!_data || !_data->command_pool)
        throw std::invalid_argument("Command manager is empty");
    return CommandPool{CommandAccess::Pool(_data->command_pool)};
}

VSL_NAMESPACE::CommandBuffer VSL_NAMESPACE::CommandManager::get_buffer() {
    if (!_data || !_data->command_buffer)
        throw std::invalid_argument("Command manager is empty");
    // private構築や破損検証でも、既存Domainを解決するだけで再生成しない。
    if (!_domain || _domain->buffer != _data->command_buffer)
        return CommandBuffer{CommandAccess::RawBuffer(_data->command_buffer)};
    if (_domain->buffer.owner_before(_data->command_buffer) || _data->command_buffer.owner_before(_domain->buffer))
        throw std::logic_error("Command manager ring has a different owner");
    return lifetime::CommandAccess::Buffer(_domain);
}

std::uint32_t VSL_NAMESPACE::CommandManager::get_current_buffer_idx() {
    if (!_data || !_data->command_buffer)
        throw std::invalid_argument("Command manager is empty");
    return _data->command_buffer->current_buffer_idx;
}

VSL_NAMESPACE::CommandBuffer VSL_NAMESPACE::CommandManager::make_exclusive_buffer(size_t size) {
    if (!_data || !_data->command_pool)
        throw std::invalid_argument("Command manager is empty");
    if (size > std::numeric_limits<std::uint32_t>::max())
        throw std::length_error("Command buffer count exceeds uint32_t");
    return CommandBuffer(CommandPool{CommandAccess::Pool(_data->command_pool)}, static_cast<std::uint32_t>(size));
}
void VSL_NAMESPACE::CommandManager::wait_idle() {
    const auto domain = lifetime::CommandAccess::Owner(get_buffer());
    domain->wait_idle();
}

VSL_NAMESPACE::LogicalDeviceAccessor VSL_NAMESPACE::CommandManager::get_device() const {
    if (!_data || !_data->device || !_data->device->device)
        throw std::invalid_argument("Command manager is empty");
    return {_data->device};
}
VSL_NAMESPACE::CommandManager VSL_NAMESPACE::CommandManager::independent(std::uint32_t count) const {
    if (!count)
        throw std::invalid_argument("Command buffer count must be positive");
    CommandPool pool(get_device());
    CommandBuffer buffers(pool, count);
    auto next = FromBuffer(std::move(buffers));
    next._data->graphics_queue = _data->graphics_queue;
    next._data->present_queue = _data->present_queue;
    return next;
}

VSL_NAMESPACE::Completion VSL_NAMESPACE::CommandBuffer::completion() const {
    const auto* context = _data ? _data->recording_context : nullptr;
    return {context && context->work ? context->work->result : nullptr};
}
