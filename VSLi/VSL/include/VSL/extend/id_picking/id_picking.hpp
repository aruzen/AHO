#pragma once

#include "../../vulkan/render_pass.hpp"
#include "stage.hpp"
#include "../../vulkan/commands/render_pass_begin.hpp"

namespace VSL_NAMESPACE {
    struct IDPickingRenderPass : public ReadableRenderPass {
        static constexpr std::size_t ObjectIdAttachment = 0;

        IDPickingRenderPass(SwapchainAccessor swapchain)
                : ReadableRenderPass(swapchain, {ReadableAttachmentDefinition{
                        .format = data_format::UnsignedInt,
                        .role = ReadableAttachmentRole::ObjectId,
                        .clear = {0, 0, 0, 0},
                }}) {}

        std::uint32_t read(vsl::CommandManager manager, FenceHolder inFlight, std::uint32_t x, std::uint32_t y) {
            return ReadableRenderPass::read(ObjectIdAttachment, manager, inFlight, x, y);
        }
    };
}

namespace VSL_NAMESPACE::command {
    struct IDPickingRenderPassBegin : public ReadableRenderPassBegin {
        IDPickingRenderPassBegin(IDPickingRenderPass renderPass,
                                 FrameBufferAccessor frameBuffer,
                                 std::optional<__VSLDRGBColorAccessor<float>> color = std::nullopt)
                : ReadableRenderPassBegin(renderPass, frameBuffer, color) {}
    };
}
