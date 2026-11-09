#include <VSL/operation/graphics_pipeline_state.hpp>
#include <VSL/vulkan/pipeline_layout.hpp>
#include <VSL/vulkan/stages/rasterization.hpp>
#include <VSL/vulkan/stages/depth_stencil.hpp>
#include <type_traits>

#if VSL_TYPED_CASE == 0
static_assert(std::is_copy_constructible_v<vsl::GraphicsPipelineState>);
static_assert(std::is_copy_assignable_v<vsl::GraphicsPipelineState>);
static_assert(std::is_nothrow_move_constructible_v<vsl::GraphicsPipelineState>);
static_assert(std::is_nothrow_move_assignable_v<vsl::GraphicsPipelineState>);
static_assert(!std::is_aggregate_v<vsl::GraphicsPipelineStateEditRef>);
static_assert(std::is_trivially_copyable_v<vsl::GraphicsPipelineStateEditRef>);
struct ComposedState {
    void injection(vsl::GraphicsPipelineStateEditRef state) const {
        vsl::pipeline_layout::Rasterization{vsl::pipeline_layout::CullingMode::NONE}.injection(state);
        vsl::pipeline_layout::DepthStencil{}.injection(state);
    }
    void injection(vsl::PipelineLayoutEditAccessor layout) const { injection(layout.edit_state()); }
};
void accept_state(vsl::GraphicsPipelineState state, vsl::PipelineLayoutEditAccessor layout);
void accept_state(vsl::GraphicsPipelineState state, vsl::PipelineLayoutEditAccessor layout) {
    auto copied = state;
    copied = state;
    ComposedState{}.injection(copied.edit());
    ComposedState{}.injection(layout);
    vsl::GraphicsPipelineState composed(ComposedState{});
    composed.injection(layout);
}
#elif VSL_TYPED_CASE == 1
#include <VSL/vulkan/stages/shader_group.hpp>
void reject_shader(vsl::pipeline_layout::ShaderGroup group);
void reject_shader(vsl::pipeline_layout::ShaderGroup group) {
    vsl::GraphicsPipelineState invalid(group);
}
#elif VSL_TYPED_CASE == 2
#include <VSL/vulkan/stages/resource_binding.hpp>
void reject_binding(vsl::pipeline_layout::ResourceBinding binding);
void reject_binding(vsl::pipeline_layout::ResourceBinding binding) {
    vsl::GraphicsPipelineState invalid(binding);
}
#elif VSL_TYPED_CASE == 3
#include <VSL/vulkan/stages/push_contents.hpp>
void reject_push(vsl::pipeline_layout::PushConstants push);
void reject_push(vsl::pipeline_layout::PushConstants push) {
    vsl::GraphicsPipelineState invalid(push);
}
#elif VSL_TYPED_CASE == 4
auto invalid = vsl::GraphicsPipelineState{}.edit();
#elif VSL_TYPED_CASE == 5
const auto& invalid = static_cast<const vsl::GraphicsPipelineState&&>(vsl::GraphicsPipelineState{}).definition();
#elif VSL_TYPED_CASE == 6
#include <VSL/vulkan/pipeline_layout.hpp>
vsl::PipelineLayoutEditAccessor make_layout();
auto invalid = make_layout().edit_state();
#elif VSL_TYPED_CASE == 7
void reject_data(vsl::GraphicsPipelineStateEditRef state);
void reject_data(vsl::GraphicsPipelineStateEditRef state) {
    (void)state.data;
}
#elif VSL_TYPED_CASE == 8
void reject_get(vsl::GraphicsPipelineStateEditRef state);
void reject_get(vsl::GraphicsPipelineStateEditRef state) {
    (void)state.get();
}
#elif VSL_TYPED_CASE == 9
vsl::GraphicsPipelineStateEditRef invalid(static_cast<vsl::impl::GraphicsPipelineStateData*>(nullptr));
#elif VSL_TYPED_CASE == 10
void reject_definition(const vsl::GraphicsPipelineState& state);
void reject_definition(const vsl::GraphicsPipelineState& state) {
    (void)state.definition();
}
#endif
