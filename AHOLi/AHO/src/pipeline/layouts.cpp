//
// Created by morimoto_hibiki on 2025/10/10.
//

#include <AHO/pipeline/layouts.hpp>
#include <AHO/window.hpp>

#include <VSL/vulkan/pipeline_layout.hpp>
#include <VSL/vulkan/pipeline.hpp>
#include <VSL/vulkan/stages/color_blend.hpp>
#include <VSL/vulkan/stages/depth_stencil.hpp>
#include <VSL/vulkan/stages/input_assembly.hpp>
#include <VSL/vulkan/stages/multisample.hpp>
#include <VSL/vulkan/stages/rasterization.hpp>

namespace pl = VSL_NAMESPACE::pipeline_layout;
using namespace VSL_NAMESPACE;

PipelineLayout get_base_layout(AHO_NAMESPACE::engine::EngineAccessor* engine) {
    return {engine->data_->logical_device,
            pl::ColorBlend(),
            pl::InputAssembly(),
            pl::Multisample(),
            pl::Rasterization(),
            pl::DepthStencil()};
}

AHO_NAMESPACE::pipeline::GetBaseLayoutFunc AHO_NAMESPACE::pipeline::GET_BASE_LAYOUT = get_base_layout;
