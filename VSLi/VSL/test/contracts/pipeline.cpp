#include <VSL/vulkan/pipeline.hpp>
#include <VSL/lifetime/resource_arena.hpp>

namespace s = vsl::storage;
namespace t = vsl::typed;
namespace l = vsl::lifetime;
constexpr std::array<t::ResourceRequest, 2> REQUESTS{{{{1}, s::ResourceKind::PIPELINE}, {{2}, s::ResourceKind::PIPELINE}}};
constexpr t::ResourcePlan<2> PLAN(REQUESTS);
static_assert(PLAN.slots() == 2);
static_assert(PLAN.placement({2}).offset == s::PIPELINE_IMPL_SIZE);
static_assert(s::ResourceSlotLayout<vsl::impl::PipelineImpl>::USE_OFFSET == s::NO_SEPARATE_USE);
void connect_pipeline(vsl::LogicalDeviceAccessor device, vsl::PipelineLayoutAccessor layout,
                      vsl::RenderPassAccessor pass, const vsl::GraphicsPipelineState& state,
                      std::span<const vsl::ShaderAccessor> shaders, vsl::ShaderAccessor compute);
void connect_pipeline(vsl::LogicalDeviceAccessor device, vsl::PipelineLayoutAccessor layout,
                      vsl::RenderPassAccessor pass, const vsl::GraphicsPipelineState& state,
                      std::span<const vsl::ShaderAccessor> shaders, vsl::ShaderAccessor compute) {
    vsl::GraphicsPipeline graphics(layout.runtime(), pass, state, shaders);
    vsl::ComputePipeline selected(layout.runtime(), compute);
    vsl::GraphicsPipeline graphics_command(vsl::PipelineAccessor{graphics});
    vsl::ComputePipeline compute_command(vsl::PipelineAccessor{selected});
    const std::array<l::ResourceInit, 2> inputs{l::GraphicsPipelineInit{layout.runtime(), pass, state, {}},
                                                l::ComputePipelineInit{layout.runtime(), compute}};
    l::ResourceArena arena(device, PLAN, inputs);
    vsl::PipelineAccessor pipeline = arena.pipeline({1});
    (void)pipeline.owner_ref();
    (void)pipeline.get_device();
}
