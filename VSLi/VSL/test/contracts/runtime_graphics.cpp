#include <VSL/vulkan/pipeline.hpp>
#include <VSL/vulkan/frame_buffer.hpp>
#include <VSL/vulkan/commands/push_content.hpp>
#include <type_traits>

#ifndef VSL_TYPED_CASE
#define VSL_TYPED_CASE 0
#endif

static_assert(!std::is_aggregate_v<vsl::PipelineLayoutAccessor>);
static_assert(!std::is_aggregate_v<vsl::RenderPassAccessor>);
static_assert(!std::is_aggregate_v<vsl::PipelineAccessor>);
static_assert(!std::is_aggregate_v<vsl::FrameBufferAccessor>);
static_assert(std::is_convertible_v<vsl::PipelineLayoutEditAccessor, vsl::PipelineLayoutAccessor>);
static_assert(std::is_convertible_v<vsl::FrozenPipelineLayout, vsl::PipelineLayoutAccessor>);
static_assert(std::is_convertible_v<vsl::GraphicsPipeline, vsl::PipelineAccessor>);
static_assert(std::is_convertible_v<vsl::ComputePipeline, vsl::PipelineAccessor>);
static_assert(std::is_convertible_v<vsl::IDPickingRenderPass, vsl::RenderPassAccessor>);
static_assert(!std::is_convertible_v<vsl::PipelineLayoutEditAccessor&, vsl::PipelineLayoutAccessor&>);
static_assert(!std::is_convertible_v<vsl::FrozenPipelineLayout&, vsl::PipelineLayoutAccessor&>);
static_assert(!std::is_convertible_v<vsl::GraphicsPipeline&, vsl::PipelineAccessor&>);
static_assert(!std::is_convertible_v<vsl::ComputePipeline&, vsl::PipelineAccessor&>);
static_assert(!std::is_convertible_v<vsl::IDPickingRenderPass&, vsl::RenderPassAccessor&>);

void connect_graphics(vsl::PipelineLayoutEditAccessor editor, vsl::FrozenPipelineLayout frozen, vsl::RenderPassAccessor pass,
                      vsl::FrameBufferAccessor target, vsl::IDPickingRenderPass& picking, vsl::GraphicsPipeline graphics,
                      vsl::ComputePipeline compute, const vsl::GraphicsPipelineState& state, std::span<const vsl::ShaderAccessor> shaders);
void connect_graphics(vsl::PipelineLayoutEditAccessor editor, vsl::FrozenPipelineLayout frozen, vsl::RenderPassAccessor pass,
                      vsl::FrameBufferAccessor target, vsl::IDPickingRenderPass& picking, vsl::GraphicsPipeline graphics,
                      vsl::ComputePipeline compute, const vsl::GraphicsPipelineState& state, std::span<const vsl::ShaderAccessor> shaders) {
    vsl::PipelineLayoutAccessor layout = editor;
    auto runtime = frozen.runtime();
    layout.swap(runtime);
    auto kept = target.get_render_pass();
    (void)kept.identity();
    (void)target.identity();
    (void)target.get_device();
    (void)target.frame_count();
    (void)target.owner_ref();
    (void)picking.picking_images();
    (void)picking.picking_image(0);
    vsl::RenderPassAccessor picking_pass = picking;
    (void)picking_pass.get_device();
    vsl::PipelineAccessor first = graphics, second = compute;
    (void)first.identity();
    first.swap(second);
    vsl::GraphicsPipeline made(editor, pass, state, shaders);
    vsl::ComputePipeline selected(frozen, shaders.front());
    std::uint32_t value = 7;
    vsl::command::PushConstant push(editor, vsl::ShaderFlag::VERTEX, value);
    (void)made;
    (void)selected;
    (void)push;
}

#if VSL_TYPED_CASE == 1
void rejected(vsl::PipelineLayoutAccessor value);
void rejected(vsl::PipelineLayoutAccessor value) {
    (void)value.data_;
}
#elif VSL_TYPED_CASE == 2
void rejected(vsl::PipelineLayoutAccessor value);
void rejected(vsl::PipelineLayoutAccessor value) {
    (void)value._owner;
}
#elif VSL_TYPED_CASE == 3
void rejected(std::shared_ptr<vsl::impl::PipelineLayoutImpl> value);
void rejected(std::shared_ptr<vsl::impl::PipelineLayoutImpl> value) {
    (void)vsl::PipelineLayoutAccessor(std::move(value));
}
#elif VSL_TYPED_CASE == 4
void rejected(vsl::RenderPassAccessor value);
void rejected(vsl::RenderPassAccessor value) {
    (void)value.data_;
}
#elif VSL_TYPED_CASE == 5
void rejected(vsl::RenderPassAccessor value);
void rejected(vsl::RenderPassAccessor value) {
    (void)value._owner;
}
#elif VSL_TYPED_CASE == 6
void rejected(std::shared_ptr<vsl::impl::RenderPassImpl> value);
void rejected(std::shared_ptr<vsl::impl::RenderPassImpl> value) {
    (void)vsl::RenderPassAccessor(std::move(value));
}
#elif VSL_TYPED_CASE == 7
void rejected(vsl::PipelineAccessor value);
void rejected(vsl::PipelineAccessor value) {
    (void)value.data_;
}
#elif VSL_TYPED_CASE == 8
void rejected(vsl::PipelineAccessor value);
void rejected(vsl::PipelineAccessor value) {
    (void)value._owner;
}
#elif VSL_TYPED_CASE == 9
void rejected(std::shared_ptr<vsl::impl::PipelineImpl> value);
void rejected(std::shared_ptr<vsl::impl::PipelineImpl> value) {
    (void)vsl::PipelineAccessor(std::move(value));
}
#elif VSL_TYPED_CASE == 10
void rejected(vsl::FrameBufferAccessor value);
void rejected(vsl::FrameBufferAccessor value) {
    (void)value.data_;
}
#elif VSL_TYPED_CASE == 11
void rejected(vsl::FrameBufferAccessor value);
void rejected(vsl::FrameBufferAccessor value) {
    (void)value._owner;
}
#elif VSL_TYPED_CASE == 12
void rejected(std::shared_ptr<vsl::impl::FrameBufferImpl> value);
void rejected(std::shared_ptr<vsl::impl::FrameBufferImpl> value) {
    (void)vsl::FrameBufferAccessor(std::move(value));
}
#elif VSL_TYPED_CASE == 13
void rejected(vsl::PipelineLayoutEditAccessor& value);
void rejected(vsl::PipelineLayoutEditAccessor& value) {
    vsl::PipelineLayoutAccessor& base = value;
}
#elif VSL_TYPED_CASE == 14
void rejected(vsl::PipelineLayout& value);
void rejected(vsl::PipelineLayout& value) {
    vsl::PipelineLayoutAccessor& base = value;
}
#elif VSL_TYPED_CASE == 15
void rejected(vsl::PipelineLayoutEditAccessor& value);
void rejected(vsl::PipelineLayoutEditAccessor& value) {
    (void)value._value;
}
#elif VSL_TYPED_CASE == 16
void rejected(vsl::IDPickingRenderPass& value);
void rejected(vsl::IDPickingRenderPass& value) {
    vsl::RenderPassAccessor& base = value;
}
#elif VSL_TYPED_CASE == 17
void rejected(vsl::IDPickingRenderPass& value);
void rejected(vsl::IDPickingRenderPass& value) {
    (void)value._picking_buffer;
}
#elif VSL_TYPED_CASE == 18
void rejected(vsl::IDPickingRenderPass value);
void rejected(vsl::IDPickingRenderPass value) {
    (void)std::move(value).picking_images();
}
#elif VSL_TYPED_CASE == 19
bool rejected(vsl::RenderPassIdentity pass, vsl::FrameBufferIdentity target);
bool rejected(vsl::RenderPassIdentity pass, vsl::FrameBufferIdentity target) {
    return pass == target;
}
#elif VSL_TYPED_CASE == 20
void rejected(vsl::GraphicsPipeline& value);
void rejected(vsl::GraphicsPipeline& value) {
    vsl::PipelineAccessor& base = value;
}
#elif VSL_TYPED_CASE == 21
void rejected(vsl::ComputePipeline& value);
void rejected(vsl::ComputePipeline& value) {
    vsl::PipelineAccessor& base = value;
}
#elif VSL_TYPED_CASE == 22
void rejected(vsl::FrozenPipelineLayout& value);
void rejected(vsl::FrozenPipelineLayout& value) {
    vsl::PipelineLayoutAccessor& base = value;
}
#endif
