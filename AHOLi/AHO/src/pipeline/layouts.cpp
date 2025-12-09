//
// Created by morimoto_hibiki on 2025/10/10.
//

#include <AHO/pipeline/layouts.hpp>
#include <AHO/window.hpp>

#include <VSL/vulkan/pipeline_layout.hpp>
#include <VSL/vulkan/pipeline.hpp>


namespace pl = VSL_NAMESPACE::pipeline_layout;
using namespace VSL_NAMESPACE;

PipelineLayout getBaseLayout(AHO_NAMESPACE::engine::EngineAccessor *engine) {
    return {engine->_data->logical_device,
            pl::ColorBlend(),
            pl::InputAssembly(),
            pl::Multisample(),
            pl::Rasterization(),
            pl::DepthStencil()};
}

AHO_NAMESPACE::pipeline::GET_BASE_LAYOUT_func AHO_NAMESPACE::pipeline::GET_BASE_LAYOUT = getBaseLayout;
