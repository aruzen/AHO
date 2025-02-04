#pragma once
#include "../define.h"
#include "pv.h"

#include "device.h"
#include "swapchain.h"
#include "view.h"
#include "render_pass.h"

namespace VSL_NAMESPACE {
	struct FrameBufferAccessor {
		std::shared_ptr<_impl::FrameBuffer_impl> _data;

		void setTargetFrame(std::uint32_t frameIdx);
	};

	template<typename D = VSL_NAMESPACE::VSL_DEFAULT_DIMENTION_STRUCT>
	struct FrameBuffer : public FrameBufferAccessor {
		FrameBuffer(Swapchain swapchain, View<D> view, RenderPass render_pass);
	};
}
