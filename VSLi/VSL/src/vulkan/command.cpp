#include <VSL/define.hpp>

#include <VSL/vulkan/_pimpls.hpp>
#include <VSL/lifetime/submission_retention.hpp>
#include <VSL/exceptions.hpp>

#include <VSL/vulkan/vulkan.hpp>
#include <VSL/vulkan/command.hpp>
#include <VSL/vulkan/phase.hpp>
#include <VSL/vulkan/surface.hpp>
#include <limits>
#include <stdexcept>

VSL_NAMESPACE::CommandPool::CommandPool(VSL_NAMESPACE::LogicalDeviceAccessor device) {
    data_ = std::shared_ptr<VSL_NAMESPACE::impl::CommandPoolImpl>(new VSL_NAMESPACE::impl::CommandPoolImpl);
    data_->device = device.data_;

    vk::CommandPoolCreateInfo pool_info;
    pool_info.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
    pool_info.queueFamilyIndex = device.data_->graphics_family;

    data_->command_pool = device.data_->device.createCommandPool(pool_info);
}

VSL_NAMESPACE::CommandPool::CommandPool(std::shared_ptr<VSL_NAMESPACE::impl::CommandPoolImpl> data) : data_(data) {}

VSL_NAMESPACE::CommandBuffer::CommandBuffer(VSL_NAMESPACE::CommandPool pool, std::uint32_t size) {
    if (!size)
        throw std::invalid_argument("Command buffer count must be positive");
    data_ = std::shared_ptr<VSL_NAMESPACE::impl::CommandBufferImpl>(new VSL_NAMESPACE::impl::CommandBufferImpl);
    data_->command_pool = pool.data_;

    data_->command_buffers.resize(size);
    vk::CommandBufferAllocateInfo alloc_info;
    alloc_info.commandPool = pool.data_->command_pool;
    alloc_info.level = vk::CommandBufferLevel::ePrimary;
    alloc_info.commandBufferCount = size;

    data_->current_buffer_idx = 0;
    data_->command_buffers = pool.data_->device->device.allocateCommandBuffers(alloc_info);
}

VSL_NAMESPACE::CommandBuffer::CommandBuffer(std::shared_ptr<VSL_NAMESPACE::impl::CommandBufferImpl> data) : data_(data) {}

std::uint32_t VSL_NAMESPACE::CommandBuffer::get_current_buffer_idx() {
    return data_->current_buffer_idx;
}

size_t VSL_NAMESPACE::CommandBuffer::get_size() {
    return data_->command_buffers.size();
}

void VSL_NAMESPACE::CommandBuffer::next() {
    data_->current_buffer_idx = (data_->current_buffer_idx + 1) % data_->command_buffers.size();
}

void VSL_NAMESPACE::CommandBuffer::reset() {
    for (auto& buff : data_->command_buffers)
        buff.reset();
}

void VSL_NAMESPACE::CommandBuffer::reset(std::uint32_t idx) {
    data_->command_buffers.at(idx).reset();
}

VSL_NAMESPACE::impl::CommandBufferImpl::~CommandBufferImpl() {
    for (const auto& submission : submissions)
        if (submission && submission->status == Completion::Status::SUBMITTED)
            Completion{submission}.wait();
    if (!command_buffers.empty())
        command_pool->device->device.freeCommandBuffers(command_pool->command_pool, command_buffers);
}

VSL_NAMESPACE::CommandManager::CommandManager(std::shared_ptr<VSL_NAMESPACE::impl::CommandManagerImpl> data) : data_(data) {}

VSL_NAMESPACE::CommandManager::CommandManager(VSL_NAMESPACE::LogicalDeviceAccessor device) {
    data_ = std::shared_ptr<VSL_NAMESPACE::impl::CommandManagerImpl>(new VSL_NAMESPACE::impl::CommandManagerImpl);
    data_->device = device.data_;
    vk::Device de = device.data_->device;
    CommandPool pool(device);
    CommandBuffer buffer(pool);

    data_->command_pool = pool.data_;
    data_->command_buffer = buffer.data_;
    data_->graphics_queue = de.getQueue(device.data_->graphics_family, 0);
    data_->present_queue = de.getQueue(device.data_->present_family, 0);
}

VSL_NAMESPACE::CommandPool VSL_NAMESPACE::CommandManager::get_pool() {
    return CommandPool{data_->command_pool};
}

VSL_NAMESPACE::CommandBuffer VSL_NAMESPACE::CommandManager::get_buffer() {
    return CommandBuffer{data_->command_buffer};
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
    data_->device->device.waitIdle();
}

VSL_NAMESPACE::impl::CommandPoolImpl::~CommandPoolImpl() {
    if (command_pool)
        device->device.destroyCommandPool(command_pool);
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
    auto next = std::make_shared<impl::CommandManagerImpl>();
    next->device = data_->device;
    next->command_pool = pool.data_;
    next->command_buffer = buffers.data_;
    next->graphics_queue = data_->graphics_queue;
    next->present_queue = data_->present_queue;
    return CommandManager(next);
}

void VSL_NAMESPACE::CommandBuffer::retain(std::shared_ptr<void> resource) const {
    if (!data_)
        throw std::invalid_argument("CommandBuffer is empty");
    if (resource && data_->recording_submission)
        Completion{data_->recording_submission}.retain(std::move(resource));
}

VSL_NAMESPACE::Completion VSL_NAMESPACE::CommandBuffer::completion() const {
    return {data_ ? data_->recording_submission : nullptr};
}

void VSL_NAMESPACE::CommandBuffer::retain_until_complete(std::shared_ptr<void> resource) const {
    if (!data_ || !data_->recording_submission || data_->recording_submission->status != Completion::Status::PENDING)
        throw std::logic_error("Buffer retention requires a recording phase");
    if (resource.use_count()) {
        data_->recording_submission->release_resources = lifetime::release_completed_resources;
        auto& resources = data_->recording_submission->completion_resources;
        if (!std::ranges::any_of(resources, [&](const auto& existing) { return !existing.owner_before(resource) && !resource.owner_before(existing); }))
            resources.push_back(std::move(resource));
    }
}
