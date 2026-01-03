#pragma once
#include "../define.hpp"
#include "pv.hpp"

#include "device.hpp"
#include "swapchain.hpp"
#include "render_pass.hpp"

namespace VSL_NAMESPACE {
	struct FrameBufferAccessor {
		std::shared_ptr<_impl::FrameBuffer_impl> _data;
	};

	struct FrameBuffer : public FrameBufferAccessor {
		FrameBuffer(SwapchainAccessor swapchain, RenderPass render_pass);
        FrameBuffer(SwapchainAccessor swapchain, IDPickingRenderPass render_pass);
	};
}
