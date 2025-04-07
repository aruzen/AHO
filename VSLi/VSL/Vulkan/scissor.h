#pragma once
#include "../define.h"
#include "pv.h"

#include "swapchain.h"
#include "../utils/VSLArray.h"
#include "pipeline_layout.h"
#include "command.h"

namespace VSL_NAMESPACE {
	struct Scissor {
		int32_t x = 0, y = 0;
		uint32_t width = 1, height = 1;

		// std::shared_ptr<_impl::Scissor_impl> _data;

		Scissor();
		Scissor(float x, float y, float width, float height);
		Scissor(__VSLD2RectangleAccessor<float> rect);
		Scissor(SwapchainAccessor swapchain);

		void injection(VSL_NAMESPACE::PipelineLayoutAccessor pl);
		void invoke(CommandPool pool, CommandBuffer buffer, CommandManager manager);
	};
}