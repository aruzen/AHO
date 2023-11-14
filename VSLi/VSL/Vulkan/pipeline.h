#pragma once
#include "../define.h"
#include "pv.h"

#include "pipeline_layout.h"
#include "render_pass.h"

namespace VSL_NAMESPACE {
	struct Pipeline {
		Pipeline(PipelineLayoutAccesor layout, RenderPass pass);

		std::shared_ptr<_impl::Pipeline_impl> _data;
	};
}