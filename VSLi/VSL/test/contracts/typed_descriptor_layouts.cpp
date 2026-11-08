#include <VSL/lifetime/typed_descriptor_layouts.hpp>
#include <VSL/utils/typed_resource_bundle.hpp>
#include <VSL/utils/typed_pipeline.hpp>
#include <VSL/utils/typed_compute.hpp>
#include <VSL/vulkan/stages/shader_group.hpp>

#ifndef VSL_TYPED_CASE
#define VSL_TYPED_CASE 0
#endif
namespace {
    namespace t = vsl::typed;
    using Resources = t::ResourceSchema<t::DescriptorSet<2, t::UniformBinding<3, float, vsl::ShaderFlag::VERTEX>>>;
    using Other = t::ResourceSchema<t::DescriptorSet<3, t::UniformBinding<3, float, vsl::ShaderFlag::VERTEX>>>;
    using Layouts = t::TypedDescriptorLayouts<Resources>;
    using Allocation = t::ResourceAllocationSchema<Resources, t::BufferAllocation<2, 3>>;
    using Bundle = t::TypedResourceBundle<Allocation, t::ArenaStorage<>>;
    using Input = t::TypedVertexInput<>;
#if VSL_TYPED_CASE == 0
    void accepted(vsl::LogicalDeviceAccessor device, vsl::RenderPassAccessor pass, const Layouts& layouts, vsl::pipeline_layout::ShaderGroup shaders, Bundle& bundle) {
        t::TypedComputePipeline<Resources> compute(device, layouts, shaders);
        t::TypedGraphicsPipeline<Input, Resources> graphics(device, pass, layouts, shaders);
        auto source = layouts.layout<2>();
        auto owning = std::move(bundle).descriptor_layouts();
        auto moved = std::move(owning);
        owning = layouts;
    }
#elif VSL_TYPED_CASE == 1
    void rejected(vsl::LogicalDeviceAccessor device, const Layouts& layouts, vsl::pipeline_layout::ShaderGroup shaders) {
        t::TypedComputePipeline<Other> pipeline(device, layouts, shaders);
    }
#elif VSL_TYPED_CASE == 2
    void rejected(vsl::LogicalDeviceAccessor device, vsl::RenderPassAccessor pass, const Layouts& layouts, vsl::pipeline_layout::ShaderGroup shaders) {
        t::TypedGraphicsPipeline<Input, Other> pipeline(device, pass, layouts, shaders);
    }
#elif VSL_TYPED_CASE == 3
    auto rejected(const Layouts& layouts) {
        return layouts.layout<99>();
    }
#elif VSL_TYPED_CASE == 4
    auto rejected(const t::TypedResourceBundle<Allocation, t::HeapStorage>& bundle) {
        return bundle.descriptor_layouts();
    }
#elif VSL_TYPED_CASE == 5
    void rejected(vsl::LogicalDeviceAccessor device, const Layouts& layouts) {
        vsl::PipelineLayout pipeline(device, layouts);
    }
#endif
} // namespace
