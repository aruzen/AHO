#include <VSL/operation/graphics_pipeline_state_access.hpp>
#include <VSL/lifetime/swapchain_access.hpp>
#include <VSL/lifetime/command_access.hpp>
#include <VSL/vulkan/scissor.hpp>

#include <VSL/vulkan/_pimpls.hpp>

namespace {
    using VSL_NAMESPACE::operation::detail::GraphicsPipelineStateAccess;
}

namespace {
    using VSL_NAMESPACE::lifetime::detail::SwapchainAccess;
}

namespace {
    using VSL_NAMESPACE::lifetime::CommandAccess;
}
VSL_NAMESPACE::Scissor::Scissor() {}

VSL_NAMESPACE::Scissor::Scissor(float x, float y, float width, float height) : x(x), y(y), width(width), height(height) {}

VSL_NAMESPACE::Scissor::Scissor(VSLD2RectangleAccessor<float> rect) : x(rect.x), y(rect.y), width(rect.width), height(rect.height) {}

VSL_NAMESPACE::Scissor::Scissor(SwapchainAccessor swapchain) : x(0), y(0), height(SwapchainAccess::Owned(swapchain)->swap_chain_extent.height), width(SwapchainAccess::Owned(swapchain)->swap_chain_extent.width) {}

void VSL_NAMESPACE::Scissor::injection(VSL_NAMESPACE::PipelineLayoutEditAccessor pl) {
    injection(pl.edit_state());
}

void VSL_NAMESPACE::Scissor::injection(VSL_NAMESPACE::GraphicsPipelineStateEditRef state) {
    auto& info = GraphicsPipelineStateAccess::Edit(state);

    vk::Rect2D scissor;
    scissor.offset = vk::Offset2D{x, y};
    scissor.extent = vk::Extent2D{width, height};
    info.scissors.push_back(scissor);
}

void VSL_NAMESPACE::Scissor::invoke(CommandPool pool, CommandBuffer buffer, CommandManager manager) {
    vk::Rect2D scissor;
    scissor.offset = vk::Offset2D{x, y};
    scissor.extent = vk::Extent2D{width, height};

    CommandAccess::Data(buffer)->command_buffers[buffer.get_current_buffer_idx()].setScissor(0, scissor);
    // buffer.data_->command_buffers[buffer.get_current_buffer_idx()].setScissorWithCount({ scissor });
}
