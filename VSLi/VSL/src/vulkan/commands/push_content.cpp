//
// Created by morimoto_hibiki on 2025/10/17.
//

#include <VSL/lifetime/graphics_access.hpp>
#include <VSL/lifetime/owner_access.hpp>
#include <VSL/lifetime/command_access.hpp>
#include <VSL/vulkan/commands/push_content.hpp>
#include <VSL/vulkan/_pimpls.hpp>

namespace {
    using VSL_NAMESPACE::lifetime::detail::GraphicsAccess;
}

namespace {
    using VSL_NAMESPACE::lifetime::CommandAccess;
}
void VSL_NAMESPACE::command::PushConstant::invoke(VSL_NAMESPACE::CommandPool pool, VSL_NAMESPACE::CommandBuffer buffer, VSL_NAMESPACE::CommandManager manager) {
    buffer.retain(lifetime::detail::OwnerAccess::Shared(GraphicsAccess::Owned(layout)));
    CommandAccess::Data(buffer)->command_buffers[buffer.get_current_buffer_idx()].pushConstants(GraphicsAccess::Owned(layout)->pipeline_layout, (vk::ShaderStageFlagBits)target_type, offset, size, value);
}
