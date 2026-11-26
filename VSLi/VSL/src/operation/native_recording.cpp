#include <VSL/operation/native_recording.hpp>
#include <VSL/vulkan/_pimpls.hpp>
#include <stdexcept>

namespace VSL_NAMESPACE::operation {
    VkCommandBuffer native_command_buffer(const impl::CommandBufferImpl& buffer, std::uint32_t slot) {
        if (slot >= buffer.command_buffers.size() || !buffer.command_buffers[slot])
            throw std::logic_error("Native recording requires a constructed command buffer slot");
        return static_cast<VkCommandBuffer>(buffer.command_buffers[slot]);
    }
} // namespace VSL_NAMESPACE::operation
