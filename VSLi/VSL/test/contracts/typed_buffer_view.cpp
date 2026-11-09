#include <VSL/utils/typed_pipeline.hpp>
#ifndef VSL_TYPED_CASE
#define VSL_TYPED_CASE 0
#endif
namespace {
    namespace t = vsl::typed;
    struct Vertex {
        float xy[2];
    };
    struct Wrong {
        float xy[2];
    };
    using Input = t::TypedVertexInput<t::VertexRecord<Vertex, VSL_VERTEX_MEMBER(Vertex, xy)>::Binding<0>>;
    using Pipeline = t::TypedGraphicsPipeline<Input>;
    using Set = t::DescriptorLayout<t::UniformBinding<0, float, vsl::ShaderFlag::VERTEX>>;
    template<typename Buffer>
    concept rvalue_backend = requires(Buffer&& buffer) { std::move(buffer).backend(); };
#if VSL_TYPED_CASE == 0
    static_assert(!rvalue_backend<t::TypedVertexBuffer<Vertex>> && !rvalue_backend<const t::TypedVertexBuffer<Vertex>>);
    static_assert(!rvalue_backend<t::TypedVertexBufferView<Vertex>> && !rvalue_backend<const t::TypedVertexBufferView<Vertex>>);
    static_assert(!rvalue_backend<t::TypedIndexBuffer<std::uint32_t>> && !rvalue_backend<const t::TypedIndexBuffer<std::uint32_t>>);
    static_assert(!rvalue_backend<t::TypedUniformBuffer<float>> && !rvalue_backend<const t::TypedUniformBuffer<float>>);
    static_assert(std::is_constructible_v<t::TypedVertexBuffer<Vertex>, vsl::LogicalDeviceAccessor, std::span<const Vertex>, t::HeapStorage>);
    static_assert(std::is_constructible_v<t::TypedVertexBuffer<Vertex>, vsl::LogicalDeviceAccessor, std::span<const Vertex>, t::ArenaStorage<>>);
    static_assert(!std::is_constructible_v<t::TypedVertexBuffer<Vertex>, vsl::LogicalDeviceAccessor, std::span<const Vertex>, t::ArenaStorage<int>>);
    static_assert(t::compatible_vertex_buffers<Input, t::TypedVertexBufferView<Vertex>>);
    static_assert(t::TypedGraphicsPipeline<Input, t::ResourceSchema<>, t::PushSchema<>, 1>::SUBPASS == 1);
    void accepted(Pipeline& pipeline, const t::TypedVertexBuffer<Vertex>& buffer, t::TypedDescriptorSet<Set>& set, const t::TypedUniformBuffer<float>& uniform) {
        pipeline.bind_vertices(buffer.view(1, 3)).draw(3);
        set.write<0>(uniform.view(1, 1));
    }
#elif VSL_TYPED_CASE == 1
    void rejected(Pipeline& pipeline, const t::TypedVertexBufferView<Wrong>& view) {
        pipeline.bind_vertices(view);
    }
#elif VSL_TYPED_CASE == 2
    void rejected(t::TypedDescriptorSet<Set>& set, const t::TypedStorageBuffer<float>& buffer) {
        set.write<0>(buffer.view(1, 1));
    }
#elif VSL_TYPED_CASE == 3
    void rejected(t::TypedDescriptorSet<Set>& set, const t::TypedUniformBuffer<std::uint32_t>& buffer) {
        set.write<0>(buffer.view(1, 1));
    }
#elif VSL_TYPED_CASE == 4
    void rejected(Pipeline& pipeline, const vsl::BufferAccessor& buffer) {
        pipeline.bind_vertices(buffer);
    }
#elif VSL_TYPED_CASE == 5
    static_assert(sizeof(t::TypedGraphicsPipeline<Input, t::ResourceSchema<>, t::PushSchema<>, -1>) > 0);
#elif VSL_TYPED_CASE == 6
    void rejected(t::TypedVertexBuffer<Vertex>& buffer) {
        std::move(buffer).backend();
    }
#elif VSL_TYPED_CASE == 7
    void rejected(const t::TypedUniformBuffer<float>& buffer) {
        std::move(buffer).backend();
    }
#elif VSL_TYPED_CASE == 8
    void rejected(vsl::LogicalDeviceAccessor device, std::span<const Vertex> values) {
        t::TypedVertexBuffer<Vertex> buffer(device, values, t::ArenaStorage<int>{});
    }
#endif
} // namespace
