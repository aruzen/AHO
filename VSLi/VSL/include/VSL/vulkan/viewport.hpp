#pragma once
#include "../define.hpp"
#include "pv.hpp"

#include "swapchain.hpp"
#include "../utils/vsl_array.hpp"
#include "pipeline_layout.hpp"
#include "command.hpp"

namespace VSL_NAMESPACE {
    struct Viewport {
        float x = 0, y = 0, width = 1, height = 1;

        // std::shared_ptr<impl::ViewportImpl> data_;

        Viewport();
        Viewport(float x, float y, float width, float height);
        Viewport(VSLD2RectangleAccessor<float> rect);
        Viewport(SwapchainAccessor swapchain);

        void injection(VSL_NAMESPACE::PipelineLayoutEditAccessor pl);
        void invoke(CommandPool pool, CommandBuffer buffer, CommandManager manager);
    };
} // namespace VSL_NAMESPACE
