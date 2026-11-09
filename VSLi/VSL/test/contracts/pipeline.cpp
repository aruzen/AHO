#include <VSL/vulkan/pipeline.hpp>
#include <VSL/lifetime/resource_arena.hpp>
#include <VSL/vulkan/stages/shader_group.hpp>
#include <type_traits>

#ifndef VSL_TYPED_CASE
#define VSL_TYPED_CASE 0
#endif

static_assert(!std::is_aggregate_v<vsl::ShaderAccessor>);
static_assert(!std::is_aggregate_v<vsl::pipeline_layout::ShaderGroup>);
static_assert(std::is_default_constructible_v<vsl::ShaderAccessor>);
static_assert(std::is_copy_constructible_v<vsl::ShaderAccessor>);
static_assert(std::is_nothrow_move_constructible_v<vsl::ShaderAccessor>);

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

#if VSL_TYPED_CASE == 1
void reject_shader(vsl::ShaderAccessor shader);
void reject_shader(vsl::ShaderAccessor shader) {
    (void)shader.data_;
}
#elif VSL_TYPED_CASE == 2
void reject_shader(vsl::ShaderAccessor shader);
void reject_shader(vsl::ShaderAccessor shader) {
    (void)shader._owner;
}
#elif VSL_TYPED_CASE == 3
void reject_shader(std::shared_ptr<vsl::impl::ShaderImpl> shader);
void reject_shader(std::shared_ptr<vsl::impl::ShaderImpl> shader) {
    (void)vsl::ShaderAccessor(std::move(shader));
}
#elif VSL_TYPED_CASE == 4
void reject_shader(vsl::pipeline_layout::ShaderGroup group);
void reject_shader(vsl::pipeline_layout::ShaderGroup group) {
    (void)group.data_;
}
#elif VSL_TYPED_CASE == 5
void reject_shader(vsl::pipeline_layout::ShaderGroup group);
void reject_shader(vsl::pipeline_layout::ShaderGroup group) {
    (void)group._data;
}
#elif VSL_TYPED_CASE == 6
void reject_shader(vsl::ShaderAccessor shader);
void reject_shader(vsl::ShaderAccessor shader) {
    (void)std::move(shader).code();
}
#elif VSL_TYPED_CASE == 7
void reject_shader(vsl::ShaderAccessor shader);
void reject_shader(vsl::ShaderAccessor shader) {
    (void)std::move(std::as_const(shader)).code();
}
#endif
