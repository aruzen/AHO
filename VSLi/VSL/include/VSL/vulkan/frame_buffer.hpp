#pragma once
#include "../define.hpp"
#include "pv.hpp"

#include "device.hpp"
#include "swapchain.hpp"
#include "view.hpp"
#include "render_pass.hpp"

namespace VSL_NAMESPACE {
	struct FrameBufferAccessor {
		std::shared_ptr<_impl::FrameBuffer_impl> _data;

		void setTargetFrame(std::uint32_t frameIdx);
	};

	template<typename D = VSL_NAMESPACE::VSL_DEFAULT_dimension_STRUCT>
	struct FrameBuffer : public FrameBufferAccessor {
		FrameBuffer(SwapchainAccessor swapchain, View<D> view, RenderPass render_pass);
	};
}
