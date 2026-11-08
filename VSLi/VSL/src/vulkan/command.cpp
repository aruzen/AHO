#include <VSL/define.hpp>

#include <VSL/vulkan/_pimpls.hpp>
#include <VSL/exceptions.hpp>

#include <VSL/vulkan/vulkan.hpp>
#include <VSL/vulkan/command.hpp>
#include <VSL/vulkan/phase.hpp>
#include <VSL/vulkan/surface.hpp>
#include <VSL/lifetime/command_domain.hpp>
#include <VSL/lifetime/command_storage.hpp>
#include <VSL/lifetime/owner_access.hpp>
#include <limits>
#include <stdexcept>

VSL_NAMESPACE::CommandPool::CommandPool(VSL_NAMESPACE::LogicalDeviceAccessor device, CommandPoolCreateInfo info)
    : data_(lifetime::make_heap_command_pool(device, info)) {}

VSL_NAMESPACE::LogicalDeviceAccessor VSL_NAMESPACE::CommandPool::get_device() const {
    (void)owner_ref();
    if (!data_->device || !data_->native_device || !data_->command_pool)
        throw std::invalid_argument("Command pool is empty");
    return {data_->device->shared_from_this()};
}
VSL_NAMESPACE::lifetime::OwnerRef VSL_NAMESPACE::CommandPool::owner_ref() const {
    if (!data_ || !data_.use_count() || !data_->owner_node || !data_->owner_node->matches(data_))
        throw std::invalid_argument("Command pool requires its canonical owner");
    return lifetime::detail::owned_ref(data_, data_->owner_node);
}

VSL_NAMESPACE::CommandPool::CommandPool(std::shared_ptr<VSL_NAMESPACE::impl::CommandPoolImpl> data) : data_(data) {}

VSL_NAMESPACE::CommandBuffer::CommandBuffer(VSL_NAMESPACE::CommandPool pool, std::uint32_t size) {
    *this = lifetime::CommandAccess::Attach(lifetime::make_heap_command_buffer(pool, size));
}

VSL_NAMESPACE::CommandBuffer::CommandBuffer(std::shared_ptr<VSL_NAMESPACE::impl::CommandBufferImpl> data)
    : data_(std::move(data)), _domain(data_ ? lifetime::CommandAccess::Resolve(data_) : nullptr) {}

VSL_NAMESPACE::CommandBuffer& VSL_NAMESPACE::CommandBuffer::operator=(CommandBuffer other) noexcept {
    swap(other);
    return *this;
}
void VSL_NAMESPACE::CommandBuffer::swap(CommandBuffer& other) noexcept {
    data_.swap(other.data_);
    _domain.swap(other._domain);
}
VSL_NAMESPACE::CommandPool VSL_NAMESPACE::CommandBuffer::get_pool() const {
    (void)owner_ref();
    if (!data_->owner_context || !data_->command_pool)
        throw std::invalid_argument("Command ring is empty");
    auto pool = data_->owner_context->pool.lock();
    if (!pool.use_count() || pool.get() != data_->command_pool)
        throw std::logic_error("Command ring pool owner has expired");
    return {pool.share()};
}
VSL_NAMESPACE::lifetime::OwnerRef VSL_NAMESPACE::CommandBuffer::owner_ref() const {
    if (!data_ || !data_.use_count() || !data_->owner_node || !data_->owner_node->matches(data_))
        throw std::invalid_argument("Command ring requires its canonical owner");
    return lifetime::detail::owned_ref(data_, data_->owner_node);
}

std::uint32_t VSL_NAMESPACE::CommandBuffer::get_current_buffer_idx() {
    return data_->current_buffer_idx;
}

size_t VSL_NAMESPACE::CommandBuffer::get_size() {
    return data_->command_buffers.size();
}

void VSL_NAMESPACE::CommandBuffer::next() {
    lifetime::next_command_buffer(*this);
}

void VSL_NAMESPACE::CommandBuffer::reset() {
    lifetime::reset_command_buffers(*this, std::nullopt);
}

void VSL_NAMESPACE::CommandBuffer::reset(std::uint32_t idx) {
    lifetime::reset_command_buffers(*this, idx);
}

VSL_NAMESPACE::CommandManager::CommandManager(std::shared_ptr<VSL_NAMESPACE::impl::CommandManagerImpl> data)
    : data_(std::move(data)), _domain(data_ ? lifetime::CommandAccess::Resolve(data_->command_buffer) : nullptr) {}

VSL_NAMESPACE::CommandManager& VSL_NAMESPACE::CommandManager::operator=(CommandManager other) noexcept {
    swap(other);
    return *this;
}
void VSL_NAMESPACE::CommandManager::swap(CommandManager& other) noexcept {
    data_.swap(other.data_);
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
    data->command_pool = pool.data_;
    data->command_buffer = buffer.data_;
    data->graphics_queue = device.data_->device.getQueue(device.data_->graphics_family, 0);
    data->present_queue = device.data_->device.getQueue(device.data_->present_family, 0);
    return lifetime::CommandAccess::Manager(std::move(data), domain);
}

VSL_NAMESPACE::CommandPool VSL_NAMESPACE::CommandManager::get_pool() {
    return CommandPool{data_->command_pool};
}

VSL_NAMESPACE::CommandBuffer VSL_NAMESPACE::CommandManager::get_buffer() {
    if (!data_ || !data_->command_buffer)
        throw std::invalid_argument("Command manager is empty");
    // data_直接差替えの互換入口だけで、元ringのDomainを上書きしない。
    if (!_domain || _domain->buffer != data_->command_buffer)
        return CommandBuffer{data_->command_buffer};
    if (_domain->buffer.owner_before(data_->command_buffer) || data_->command_buffer.owner_before(_domain->buffer))
        throw std::logic_error("Command manager ring has a different owner");
    return lifetime::CommandAccess::Buffer(_domain);
}

std::uint32_t VSL_NAMESPACE::CommandManager::get_current_buffer_idx() {
    return data_->command_buffer->current_buffer_idx;
}

VSL_NAMESPACE::CommandBuffer VSL_NAMESPACE::CommandManager::make_exclusive_buffer(size_t size) {
    if (size > std::numeric_limits<std::uint32_t>::max())
        throw std::length_error("Command buffer count exceeds uint32_t");
    return CommandBuffer(CommandPool{data_->command_pool}, static_cast<std::uint32_t>(size));
}
void VSL_NAMESPACE::CommandManager::wait_idle() {
    const auto domain = lifetime::CommandAccess::Owner(get_buffer());
    domain->wait_idle();
}

VSL_NAMESPACE::LogicalDeviceAccessor VSL_NAMESPACE::CommandManager::get_device() const {
    if (!data_ || !data_->device || !data_->device->device)
        throw std::invalid_argument("Command manager is empty");
    return {data_->device};
}
VSL_NAMESPACE::CommandManager VSL_NAMESPACE::CommandManager::independent(std::uint32_t count) const {
    if (!count)
        throw std::invalid_argument("Command buffer count must be positive");
    CommandPool pool(get_device());
    CommandBuffer buffers(pool, count);
    auto next = FromBuffer(std::move(buffers));
    next.data_->graphics_queue = data_->graphics_queue;
    next.data_->present_queue = data_->present_queue;
    return next;
}

VSL_NAMESPACE::Completion VSL_NAMESPACE::CommandBuffer::completion() const {
    const auto* context = data_ ? data_->recording_context : nullptr;
    return {context && context->work ? context->work->result : nullptr};
}
