#include <VSL/utils/typed_pipeline.hpp>
#include <VSL/utils/typed_compute.hpp>
#include <VSL/vulkan/stages/color_blend.hpp>
#include <VSL/vulkan/stages/rasterization.hpp>
#include <VSL/vulkan/stages/shader_group.hpp>

#ifndef VSL_TYPED_CASE
#define VSL_TYPED_CASE 0
#endif

namespace recipe_contract {
    namespace t = vsl::typed;
    struct Vertex {
        float xy[2];
    };
    using Record = t::VertexRecord<Vertex, VSL_VERTEX_MEMBER(Vertex, xy)>;
    using Input = t::TypedVertexInput<Record::Binding<0>>;
    using Graphics = t::TypedGraphicsPipeline<Input>;
    struct ComposedStages {
        void injection(vsl::PipelineLayoutEditAccessor layout) const {
            vsl::pipeline_layout::ColorBlend{}.injection(layout);
            vsl::pipeline_layout::Rasterization{vsl::pipeline_layout::CullingMode::NONE}.injection(layout);
        }
    };
    void connect_recipe(vsl::LogicalDeviceAccessor device, vsl::RenderPassAccessor pass,
                        vsl::ShaderAccessor vertex, vsl::ShaderAccessor fragment, vsl::ShaderAccessor compute);
    void connect_recipe(vsl::LogicalDeviceAccessor device, vsl::RenderPassAccessor pass,
                        vsl::ShaderAccessor vertex, vsl::ShaderAccessor fragment, vsl::ShaderAccessor compute) {
        vsl::lifetime::PipelineRecipeDraft draft(device);
        draft.inject(ComposedStages{}, vsl::pipeline_layout::ShaderGroup("draft", {vertex, fragment}));
        auto snapshot = draft.snapshot();
        (void)snapshot.pending_layouts();
        (void)snapshot.layout_sources();
        Graphics heap(t::HeapStorage{}, device, pass, ComposedStages{}, vsl::pipeline_layout::ShaderGroup("heap", {vertex, fragment}));
        Graphics arena(t::ArenaStorage<>{}, device, pass, ComposedStages{}, vsl::pipeline_layout::ShaderGroup("arena", {vertex, fragment}));
        t::TypedComputePipeline<> heap_compute(t::HeapStorage{}, device, vsl::pipeline_layout::ShaderGroup("heap compute", {compute}));
        t::TypedComputePipeline<> arena_compute(t::ArenaStorage<>{}, device, vsl::pipeline_layout::ShaderGroup("arena compute", {compute}));
        vsl::PipelineLayout layout(device, ComposedStages{});
        auto copied = layout.copy();
        auto added = copied.add(ComposedStages{});
        auto frozen = std::move(added).freeze();
        (void)frozen.runtime();
        (void)frozen.has_recipe();
        (void)heap;
        (void)arena;
        (void)heap_compute;
        (void)arena_compute;
    }
} // namespace recipe_contract

#if VSL_TYPED_CASE == 1
void rejected(const vsl::PipelineLayoutAccessor& value);
void rejected(const vsl::PipelineLayoutAccessor& value) {
    (void)value.recipe();
}
#elif VSL_TYPED_CASE == 2
void rejected(const vsl::PipelineLayoutEditAccessor& value);
void rejected(const vsl::PipelineLayoutEditAccessor& value) {
    (void)value.recipe();
}
#elif VSL_TYPED_CASE == 3
void rejected(const vsl::FrozenPipelineLayout& value);
void rejected(const vsl::FrozenPipelineLayout& value) {
    (void)value.recipe();
}
#elif VSL_TYPED_CASE == 4
void rejected(const vsl::PipelineLayoutEditAccessor& value);
void rejected(const vsl::PipelineLayoutEditAccessor& value) {
    (void)value.edit_recipe();
}
#elif VSL_TYPED_CASE == 5
void rejected(const vsl::lifetime::PipelineRecipeSnapshot& value);
void rejected(const vsl::lifetime::PipelineRecipeSnapshot& value) {
    (void)value.definition();
}
#endif
