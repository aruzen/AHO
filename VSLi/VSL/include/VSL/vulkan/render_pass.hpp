#pragma once
#include "../define.hpp"
#include "pv.hpp"

#include <memory>

#include "command.hpp"
#include "swapchain.hpp"

namespace VSL_NAMESPACE {
    struct RenderPassAccessor {
        std::shared_ptr<VSL_NAMESPACE::_impl::RenderPass_impl> _data;
    };

	struct RenderPass : public RenderPassAccessor {
		RenderPass(SwapchainAccessor swapchain);
	};

    // FIXME
    struct IDPickingRenderPass : public RenderPassAccessor {
        std::shared_ptr<VSL_NAMESPACE::_impl::RenderPass_impl> _data;
        std::vector<ImageAccessor> picking_buffer;

        IDPickingRenderPass(SwapchainAccessor swapchain);
    };
}