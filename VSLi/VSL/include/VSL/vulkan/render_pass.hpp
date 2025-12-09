#pragma once
#include "../define.hpp"
#include "pv.hpp"

#include <memory>

#include "command.hpp"
#include "swapchain.hpp"

namespace VSL_NAMESPACE {
	struct RenderPass {
		std::shared_ptr<VSL_NAMESPACE::_impl::RenderPass_impl> _data;

		RenderPass(SwapchainAccessor swapchain);
	};
}