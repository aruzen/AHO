#pragma once
#include "../command.hpp"
#include "../frame_buffer.hpp"
#include "../../utils/vsl_array.hpp"

namespace VSL_NAMESPACE::command {
    struct RenderArea {
        std::int32_t x = 0, y = 0;
        // 幅と高さがともに0ならフレームバッファ全体を対象にする。
        std::uint32_t width = 0, height = 0;
    };
    struct RenderPassBegin : __Command {
        RenderPassBegin(FrameBufferAccessor target, std::vector<ClearValue> clearValues = {},
                        RenderArea area = {}, std::uint32_t targetIndex = 0);
        RenderPassBegin(RenderPass pass, FrameBufferAccessor target,
                        std::optional<__VSLDRGBColorAccessor<float>> color = std::nullopt);
        // 未指定のアタッチメントには、形式に応じた既定のクリア値を使う。
        static RenderPassBegin withClears(FrameBufferAccessor target, std::vector<AttachmentClear> values,
                                          RenderArea area = {}, std::uint32_t targetIndex = 0);
        // 既存コードとの互換性のために保持する。
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
}
