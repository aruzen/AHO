#include <VSL/utils/typed_pipeline.hpp>
#ifndef VSL_TYPED_CASE
#define VSL_TYPED_CASE 0
#endif
namespace {
    namespace t = vsl::typed;
    using Pipeline = t::TypedGraphicsPipeline<t::TypedVertexInput<>>;
#if VSL_TYPED_CASE == 0
    static_assert(t::index_element<std::uint16_t> && t::index_element<std::uint32_t>);
    void accepted(Pipeline& pipeline, const t::TypedIndexBuffer<std::uint16_t>& indices) {
        pipeline.bind_vertices().draw_indexed(indices, 3);
    }
#elif VSL_TYPED_CASE == 1
    static_assert(sizeof(t::TypedIndexBuffer<std::uint8_t>) > 0);
#elif VSL_TYPED_CASE == 2
    static_assert(sizeof(t::TypedIndexBuffer<std::int32_t>) > 0);
#elif VSL_TYPED_CASE == 3
    static_assert(sizeof(t::TypedIndexBuffer<std::uint64_t>) > 0);
#elif VSL_TYPED_CASE == 4
    void rejected(Pipeline& pipeline, const vsl::BufferAccessor& indices) {
        pipeline.bind_vertices().draw_indexed(indices, 3);
    }
#elif VSL_TYPED_CASE == 5
    using Resources = t::ResourceSchema<t::DescriptorSet<0, t::UniformBinding<0, float, vsl::ShaderFlag::VERTEX>>>;
    void rejected(t::TypedGraphicsPipeline<t::TypedVertexInput<>, Resources>& pipeline, const t::TypedIndexBuffer<std::uint16_t>& indices) {
        pipeline.bind_vertices().draw_indexed(indices, 3);
    }
#elif VSL_TYPED_CASE == 6
    using Push = t::PushSchema<t::PushRange<float, vsl::ShaderFlag::VERTEX>>;
    void rejected(t::TypedGraphicsPipeline<t::TypedVertexInput<>, t::ResourceSchema<>, Push>& pipeline, const t::TypedIndexBuffer<std::uint16_t>& indices) {
        pipeline.bind_vertices().draw_indexed(indices, 3);
    }
#endif
} // namespace
