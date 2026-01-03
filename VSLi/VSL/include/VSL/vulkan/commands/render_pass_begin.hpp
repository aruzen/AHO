#pragma once
#include "../../define.hpp"
#include "../pv.hpp"
#include "../command.hpp"

#include "../render_pass.hpp"
#include "../swapchain.hpp"
#include "../frame_buffer.hpp"

#include "../../utils/vsl_array.hpp"

#include <optional>

namespace VSL_NAMESPACE::command {
	struct RenderPassBegin : public __Command {
		RenderPassBegin(RenderPass renderPass, FrameBufferAccessor frameBuffer, std::optional<__VSLDRGBColorAccessor<float>> color = std::nullopt);
		RenderPassBegin(RenderPass renderPass, FrameBufferAccessor frameBuffer, size_t imageIdx, std::optional<__VSLDRGBColorAccessor<float>> color = std::nullopt);

        RenderPass renderPass;
		FrameBufferAccessor frameBuffer;
		size_t target_idx;
		std::optional<__VSLDRGBColorAccessor<float>> clear_color;

		void invoke(CommandPool pool, CommandBuffer buffer, CommandManager manager);
	};

    struct IDPickingRenderPassBegin : public __Command {
        IDPickingRenderPassBegin(IDPickingRenderPass renderPass, FrameBufferAccessor frameBuffer, std::optional<__VSLDRGBColorAccessor<float>> color = std::nullopt);
        IDPickingRenderPassBegin(IDPickingRenderPass renderPass, FrameBufferAccessor frameBuffer, size_t imageIdx, std::optional<__VSLDRGBColorAccessor<float>> color = std::nullopt);

        IDPickingRenderPass renderPass;
        FrameBufferAccessor frameBuffer;
        size_t target_idx;
        std::optional<__VSLDRGBColorAccessor<float>> clear_color;

        void invoke(CommandPool pool, CommandBuffer buffer, CommandManager manager);
    };
}