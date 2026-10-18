#include <VSL/vulkan/commands/render_pass_begin.hpp>
#include <VSL/vulkan/_pimpls.hpp>
#include "../image_format.hpp"
#include <stdexcept>
#include <type_traits>

namespace vsl::command {
namespace {
std::vector<ClearValue> defaults(const RenderPassDescription &desc) {
    std::vector<ClearValue> values;
    for (const auto &a : desc.attachments) {
        switch (_impl::helper::clearKind(static_cast<vk::Format>(a.format.type()))) {
        case 1:
            values.emplace_back(ClearInt{});
            break;
        case 2:
            values.emplace_back(ClearUint{});
            break;
        case 3:
            values.emplace_back(ClearDepthStencil{});
            break;
        default:
            values.emplace_back(ClearColor{});
            break;
        }
    }
    return values;
}
vk::ClearValue convert(const ClearValue &value) {
    vk::ClearValue result{};
    std::visit(
        [&](auto v) {
            using T = decltype(v);
            if constexpr (std::is_same_v<T, ClearColor>)
                result.color = vk::ClearColorValue(std::array<float, 4>{v.r, v.g, v.b, v.a});
            else if constexpr (std::is_same_v<T, ClearInt>)
                result.color = vk::ClearColorValue(std::array<std::int32_t, 4>{v.r, v.g, v.b, v.a});
            else if constexpr (std::is_same_v<T, ClearUint>)
                result.color = vk::ClearColorValue(std::array<std::uint32_t, 4>{v.r, v.g, v.b, v.a});
            else
                result.depthStencil = {.depth = v.depth, .stencil = v.stencil};
        },
        value);
    return result;
}
}
RenderPassBegin::RenderPassBegin(FrameBufferAccessor target, std::vector<ClearValue> clears,
                                 RenderArea renderArea, std::uint32_t index)
    : frameBuffer(std::move(target)), clearValues(std::move(clears)), area(renderArea), targetIndex(index) {
    if (!frameBuffer._data || !frameBuffer._data->renderPass)
        throw std::invalid_argument("Framebuffer required");
    renderPass._data = frameBuffer._data->renderPass;
    if (index >= frameBuffer._data->swapChainFramebuffers.size())
        throw std::out_of_range("Framebuffer index");
    if (clearValues.empty())
        clearValues = defaults(renderPass.description());
    if (clearValues.size() != renderPass.description().attachments.size())
        throw std::invalid_argument("Clear count must match attachments");
    for (std::size_t i = 0; i < clearValues.size(); ++i) {
        const auto &a = renderPass.description().attachments[i];
        if ((a.load == LoadOp::Clear || a.stencilLoad == LoadOp::Clear) &&
            clearValues[i].index() != _impl::helper::clearKind(static_cast<vk::Format>(a.format.type())))
            throw std::invalid_argument("Clear type does not match attachment format");
    }
    const auto extent = frameBuffer._data->extent;
    if (!area.width && !area.height) {
        area.width = extent.width;
        area.height = extent.height;
    }
    if (area.x < 0 || area.y < 0 || !area.width || !area.height ||
        static_cast<std::uint64_t>(area.x) + area.width > extent.width ||
        static_cast<std::uint64_t>(area.y) + area.height > extent.height)
        throw std::out_of_range("Render area exceeds framebuffer");
}
RenderPassBegin::RenderPassBegin(RenderPass pass, FrameBufferAccessor target,
                                 std::optional<__VSLDRGBColorAccessor<float>> color)
    : RenderPassBegin(target) {
    if (pass._data != renderPass._data)
        throw std::invalid_argument("RenderPass/Framebuffer mismatch");
    if (color)
        clear_color.emplace(*color);
    targetIndex.reset();
    if (color)
        clearValues.front() = ClearColor{color->r, color->g, color->b, 0.f};
}
IDPickingRenderPassBegin::IDPickingRenderPassBegin(IDPickingRenderPass pass, FrameBufferAccessor target,
                                                   std::optional<__VSLDRGBColorAccessor<float>> color)
    : RenderPassBegin(target) {
    if (pass._data != renderPass._data)
        throw std::invalid_argument("RenderPass/Framebuffer mismatch");
    if (color)
        clear_color.emplace(*color);
    targetIndex.reset();
    if (color)
        clearValues.front() = ClearColor{color->r, color->g, color->b, 0.f};
}
void RenderPassBegin::invoke(CommandPool, CommandBuffer buffer, CommandManager) {
    const auto index = targetIndex.value_or(buffer._data->acquiredImageIndex.value_or(buffer.getCurrentBufferIdx()));
    if (buffer._data->recordingSubmission) {
        buffer._data->recordingSubmission->resources.push_back(frameBuffer._data);
    }
    std::vector<vk::ClearValue> values;
    for (const auto &clear : clearValues)
        values.push_back(convert(clear));
    vk::RenderPassBeginInfo info{
        .renderPass = renderPass._data->renderPass,
        .framebuffer = frameBuffer._data->swapChainFramebuffers.at(index),
        .renderArea = {.offset = {area.x, area.y}, .extent = {area.width, area.height}},
        .clearValueCount = static_cast<std::uint32_t>(values.size()),
        .pClearValues = values.data()};
    buffer._data->commandBuffers[buffer.getCurrentBufferIdx()].beginRenderPass(info,
                                                                               vk::SubpassContents::eInline);
}
void NextSubpass::invoke(CommandPool, CommandBuffer buffer, CommandManager) {
    buffer._data->commandBuffers[buffer.getCurrentBufferIdx()].nextSubpass(vk::SubpassContents::eInline);
}
}
