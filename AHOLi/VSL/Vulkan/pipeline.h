#pragma once
#include "../define.h"
#include "pv.h"

#include "pipeline_layout.h"

namespace VSL_NAMESPACE {
	struct Pipeline {
		Pipeline(PipelineLayoutAccesor layout);
	};
}