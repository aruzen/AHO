#include <VSL/vulkan/viewport.hpp>

#include <VSL/vulkan/_pimpls.hpp>

VSL_NAMESPACE::Viewport::Viewport() {}

VSL_NAMESPACE::Viewport::Viewport(float x, float y, float width, float height) : x(x), y(y), width(width), height(height) {}

VSL_NAMESPACE::Viewport::Viewport(VSLD2RectangleAccessor<float> rect) : x(rect.x), y(rect.y), width(rect.width), height(rect.height) {}

VSL_NAMESPACE::Viewport::Viewport(SwapchainAccessor swapchain) : x(0), y(0), height(swapchain.data_->swap_chain_extent.height), width(swapchain.data_->swap_chain_extent.width) {}

void VSL_NAMESPACE::Viewport::injection(VSL_NAMESPACE::PipelineLayoutEditAccessor pl) {
    auto& info = *pl.data_->info;

    vk::Viewport viewport;
    viewport.x = x;
    viewport.y = y;
    viewport.width = width;
    viewport.height = height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    info.viewports.push_back(viewport);
}

void VSL_NAMESPACE::Viewport::invoke(CommandPool pool, CommandBuffer buffer, CommandManager manager) {
    vk::Viewport viewport;
    viewport.x = x;
    viewport.y = y;
    viewport.width = width;
    viewport.height = height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    buffer.data_->command_buffers[buffer.get_current_buffer_idx()].setViewport(0, viewport);
}
