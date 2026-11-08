#include <VSL/operation/graphics_pipeline_state.hpp>
#include <type_traits>

#if VSL_TYPED_CASE == 0
static_assert(std::is_copy_constructible_v<vsl::GraphicsPipelineState>);
static_assert(std::is_copy_assignable_v<vsl::GraphicsPipelineState>);
static_assert(std::is_nothrow_move_constructible_v<vsl::GraphicsPipelineState>);
static_assert(std::is_nothrow_move_assignable_v<vsl::GraphicsPipelineState>);
void accept_state(vsl::GraphicsPipelineState state);
void accept_state(vsl::GraphicsPipelineState state) {
    auto copied = state;
    copied = state;
    (void)copied.definition();
    (void)copied.edit();
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
#endif
