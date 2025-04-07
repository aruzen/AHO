#pragma once
#include "../define.h"
#include "pv.h"

#include "swapchain.h"
#include "../utils/VSLArray.h"
#include "pipeline_layout.h"
#include "command.h"

namespace VSL_NAMESPACE {
	struct Viewport {
		float x = 0, y = 0, width = 1, height = 1;

		// std::shared_ptr<_impl::Viewport_impl> _data;

		Viewport();
		Viewport(float x, float y, float width , float height);
		Viewport(__VSLD2RectangleAccessor<float> rect);
		Viewport(SwapchainAccessor swapchain);

		void injection(VSL_NAMESPACE::PipelineLayoutAccessor pl);
		void invoke(CommandPool pool, CommandBuffer buffer, CommandManager manager);
	};
}