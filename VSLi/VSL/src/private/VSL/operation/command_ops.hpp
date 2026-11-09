#pragma once
#include <VSL/operation/command_create_info.hpp>
#include <VSL/vulkan/_pimpls.hpp>

namespace VSL_NAMESPACE::operation {
    struct CommandNativeCalls {
        void* context = nullptr;
        VkResult (*reset)(void*, VkCommandBuffer, VkCommandBufferResetFlags) noexcept = nullptr;
        VkResult (*begin)(void*, VkCommandBuffer, const VkCommandBufferBeginInfo*) noexcept = nullptr;
        VkResult (*end)(void*, VkCommandBuffer) noexcept = nullptr;
    };
    class ScopedCommandNativeCalls {
        CommandNativeCalls _calls;
        const CommandNativeCalls* _previous;

    public:
        explicit ScopedCommandNativeCalls(CommandNativeCalls calls) noexcept;
        ~ScopedCommandNativeCalls();
        ScopedCommandNativeCalls(const ScopedCommandNativeCalls&) = delete;
        ScopedCommandNativeCalls& operator=(const ScopedCommandNativeCalls&) = delete;
    };
    void initialize_command_pool(impl::CommandPoolImpl& pool, impl::LogicalDeviceImpl& device, CommandPoolCreateInfo info);
    void destroy_command_pool(impl::CommandPoolImpl& pool) noexcept;
    void initialize_command_buffer(impl::CommandBufferImpl& buffer, impl::CommandPoolImpl& pool, std::uint32_t count);
    void destroy_command_buffer(impl::CommandBufferImpl& buffer) noexcept;
    void reset_command_buffer(vk::CommandBuffer buffer);
    void begin_command_buffer(vk::CommandBuffer buffer);
    void end_command_buffer(vk::CommandBuffer buffer);
} // namespace VSL_NAMESPACE::operation
