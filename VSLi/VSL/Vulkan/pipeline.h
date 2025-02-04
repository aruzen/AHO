#pragma once
#include "../define.h"
#include "pv.h"

#include "pipeline_layout.h"
#include "render_pass.h"

namespace VSL_NAMESPACE {
	struct Pipeline {
		Pipeline(PipelineLayoutAccessor layout, RenderPass pass);

		std::shared_ptr<_impl::Pipeline_impl> _data;

		void invoke(CommandPool pool, CommandBuffer buffer, CommandManager manager);
	};
}