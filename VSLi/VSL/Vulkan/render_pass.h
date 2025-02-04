#pragma once
#include "../define.h"
#include "pv.h"

#include <memory>

#include "command.h"
#include "swapchain.h"

namespace VSL_NAMESPACE {
	struct RenderPass {
		std::shared_ptr<VSL_NAMESPACE::_impl::RenderPass_impl> _data;

		RenderPass(SwapchainAccessor swapchain);
	};
}