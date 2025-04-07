#pragma once
#include "../../define.h"
#include "../pv.h"
#include "../command.h"

#include "../render_pass.h"
#include "../swapchain.h"
#include "../frame_buffer.h"

#include "../../utils/VSLArray.h"

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
}