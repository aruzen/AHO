#include <VSL/operation/command_ops.hpp>
#include <stdexcept>

namespace VSL_NAMESPACE::operation {
    namespace {
        thread_local const CommandNativeCalls* NATIVE_CALLS = nullptr;
    }
    ScopedCommandNativeCalls::ScopedCommandNativeCalls(CommandNativeCalls calls) noexcept
        : _calls(calls), _previous(NATIVE_CALLS) {
        NATIVE_CALLS = &_calls;
    }
    ScopedCommandNativeCalls::~ScopedCommandNativeCalls() {
        NATIVE_CALLS = _previous;
    }
    void initialize_command_pool(impl::CommandPoolImpl& pool, impl::LogicalDeviceImpl& device, CommandPoolCreateInfo info) {
        if (pool.command_pool)
            throw std::logic_error("Command pool is already initialized");
        if (!device.device || !device.parent_device || !device.parent_device->device)
            throw std::invalid_argument("Command pool requires a live device");
        const auto families = device.parent_device->device.getQueueFamilyProperties();
        if (device.graphics_family >= families.size() || !(families[device.graphics_family].queueFlags & vk::QueueFlagBits::eGraphics))
            throw std::invalid_argument("Command pool requires a graphics queue family");
        const vk::CommandPoolCreateInfo native{.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer |
                                                        (info.transient ? vk::CommandPoolCreateFlagBits::eTransient : vk::CommandPoolCreateFlags{}),
                                               .queueFamilyIndex = device.graphics_family};
        pool.device = &device;
        pool.native_device = device.device;
        pool.command_pool = device.device.createCommandPool(native);
    }
    void destroy_command_pool(impl::CommandPoolImpl& pool) noexcept {
        if (pool.command_pool) {
            pool.native_device.destroyCommandPool(pool.command_pool);
            pool.command_pool = nullptr;
        }
    }
    void initialize_command_buffer(impl::CommandBufferImpl& buffer, impl::CommandPoolImpl& pool, std::uint32_t count) {
        if (!buffer.command_buffers.empty())
            throw std::logic_error("Command buffer ring is already initialized");
        if (!count || !pool.device || !pool.native_device || !pool.command_pool)
            throw std::invalid_argument("Command buffer requires a live pool and a positive count");
        buffer.command_buffers.resize(count);
        const vk::CommandBufferAllocateInfo native{.commandPool = pool.command_pool, .level = vk::CommandBufferLevel::ePrimary, .commandBufferCount = count};
        const auto result = pool.native_device.allocateCommandBuffers(&native, buffer.command_buffers.data());
        if (result != vk::Result::eSuccess) {
            // 失敗時のnative割当はVulkanが全て回収する。未公開配列を解放対象へ残さない。
            buffer.command_buffers.clear();
            vk::detail::throwResultException(result, "Command buffer allocation failed");
        }
        buffer.command_pool = &pool;
        buffer.native_device = pool.native_device;
        buffer.native_pool = pool.command_pool;
        buffer.current_buffer_idx = 0;
    }
    void destroy_command_buffer(impl::CommandBufferImpl& buffer) noexcept {
        if (!buffer.command_buffers.empty()) {
            buffer.native_device.freeCommandBuffers(buffer.native_pool, buffer.command_buffers);
            buffer.command_buffers.clear();
        }
    }
    void reset_command_buffer(vk::CommandBuffer buffer) {
        const auto result = static_cast<vk::Result>(NATIVE_CALLS && NATIVE_CALLS->reset
                                                        ? NATIVE_CALLS->reset(NATIVE_CALLS->context, buffer, 0)
                                                        : VULKAN_HPP_DEFAULT_DISPATCHER.vkResetCommandBuffer(buffer, 0));
        if (result != vk::Result::eSuccess)
            vk::detail::throwResultException(result, "Command buffer reset failed");
    }
    void begin_command_buffer(vk::CommandBuffer buffer) {
        const VkCommandBufferBeginInfo info{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        const auto result = static_cast<vk::Result>(NATIVE_CALLS && NATIVE_CALLS->begin
                                                        ? NATIVE_CALLS->begin(NATIVE_CALLS->context, buffer, &info)
                                                        : VULKAN_HPP_DEFAULT_DISPATCHER.vkBeginCommandBuffer(buffer, &info));
        if (result != vk::Result::eSuccess)
            vk::detail::throwResultException(result, "Command buffer begin failed");
    }
    void end_command_buffer(vk::CommandBuffer buffer) {
        const auto result = static_cast<vk::Result>(NATIVE_CALLS && NATIVE_CALLS->end
                                                        ? NATIVE_CALLS->end(NATIVE_CALLS->context, buffer)
                                                        : VULKAN_HPP_DEFAULT_DISPATCHER.vkEndCommandBuffer(buffer));
        if (result != vk::Result::eSuccess)
            vk::detail::throwResultException(result, "Command buffer end failed");
    }
} // namespace VSL_NAMESPACE::operation
