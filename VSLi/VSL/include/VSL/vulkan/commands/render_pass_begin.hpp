#pragma once
#include "../command.hpp"
#include "../frame_buffer.hpp"
#include "../../utils/vsl_array.hpp"

namespace VSL_NAMESPACE::command {
struct RenderArea {
    std::int32_t x = 0, y = 0;
    // Both zero means the full framebuffer.
    std::uint32_t width = 0, height = 0;
};
struct RenderPassBegin : __Command {
    RenderPassBegin(FrameBufferAccessor target, std::vector<ClearValue> clearValues = {},
                    RenderArea area = {}, std::uint32_t targetIndex = 0);
    RenderPassBegin(RenderPass pass, FrameBufferAccessor target,
                    std::optional<__VSLDRGBColorAccessor<float>> color = std::nullopt);
    // Kept for source compatibility with existing command users.
    RenderPassAccessor renderPass;
    FrameBufferAccessor frameBuffer;
    std::optional<__VSLDRGBColorAccessor<float>> clear_color;
    std::vector<ClearValue> clearValues;
    RenderArea area;
    std::optional<std::uint32_t> targetIndex;
    void invoke(CommandPool pool, CommandBuffer buffer, CommandManager manager) override;
};
struct IDPickingRenderPassBegin : RenderPassBegin {
    IDPickingRenderPassBegin(IDPickingRenderPass pass, FrameBufferAccessor target,
                             std::optional<__VSLDRGBColorAccessor<float>> color = std::nullopt);
};
struct NextSubpass : __Command {
    void invoke(CommandPool pool, CommandBuffer buffer, CommandManager manager) override;
};
} // namespace VSL_NAMESPACE::command
