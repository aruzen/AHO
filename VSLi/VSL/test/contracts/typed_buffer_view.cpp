#include <VSL/utils/typed_pipeline.hpp>
#include <VSL/lifetime/typed_buffer_arena.hpp>
#include <VSL/lifetime/typed_resource_buffer_view.hpp>
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
    using Placed = t::PlacedBufferView<std::uint32_t, vsl::MemoryType::TRANSFER_SOURCE>;
    using PlacedUniform = t::PlacedResourceBufferView<std::uint32_t, vsl::graphic_resource::Type::UNIFORM_BUFFER>;
#if VSL_TYPED_CASE == 0
    static_assert(!rvalue_backend<Placed> && !rvalue_backend<const Placed>);
    static_assert(!rvalue_backend<PlacedUniform> && !rvalue_backend<const PlacedUniform>);
    static_assert(!std::is_aggregate_v<vsl::BufferRef>);
    void use_facades(const vsl::BufferAccessor& buffer, const vsl::BufferView& view,
                     const vsl::ImageAccessor& image, const vsl::SamplerAccessor& sampler, const vsl::GraphicResource& set) {
        vsl::ResourceUse::ForBuffer(buffer);
        vsl::ResourceUse::ForBuffer(view);
        vsl::ResourceUse::ForImage(image);
        vsl::ResourceUse::ForSampler(sampler);
        vsl::ResourceUseGroup::ForDescriptor(1, set)->tracks_descriptor(set);
    }
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
#elif VSL_TYPED_CASE == 9
    void rejected(const std::shared_ptr<vsl::impl::BufferImpl>& buffer) {
        vsl::ResourceUse::ForBuffer(buffer);
    }
#elif VSL_TYPED_CASE == 10
    void rejected(const std::shared_ptr<vsl::impl::ImageImpl>& image) {
        vsl::ResourceUse::ForImage(image);
    }
#elif VSL_TYPED_CASE == 11
    void rejected(const std::shared_ptr<vsl::impl::SamplerImpl>& sampler) {
        vsl::ResourceUse::ForSampler(sampler);
    }
#elif VSL_TYPED_CASE == 12
    void rejected(const std::shared_ptr<vsl::impl::GraphicResourceImpl>& resource) {
        vsl::ResourceUseGroup::ForDescriptor(1, resource);
    }
#elif VSL_TYPED_CASE == 13
    void rejected(const vsl::ResourceUseGroup& group, const vsl::impl::GraphicResourceImpl* resource) {
        group.tracks_descriptor(resource);
    }
#elif VSL_TYPED_CASE == 14
    void rejected(vsl::BufferRef ref) {
        ref.data;
    }
#elif VSL_TYPED_CASE == 15
    void rejected(vsl::impl::BufferImpl* data) {
        vsl::BufferRef ref{data};
    }
#elif VSL_TYPED_CASE == 16
    void rejected(vsl::BufferRef ref, vsl::lifetime::OwnerRef owner) {
        vsl::BufferView view(ref, owner, 0, 4);
    }
#elif VSL_TYPED_CASE == 17
    void rejected(const vsl::BufferView& view) {
        view.legacy_owner();
    }
#elif VSL_TYPED_CASE == 18
    void rejected(vsl::BufferView view) {
        std::move(view).ref();
    }
#elif VSL_TYPED_CASE == 19
    void rejected(Placed view) {
        std::move(view).backend().ref();
    }
#elif VSL_TYPED_CASE == 20
    void rejected(PlacedUniform view) {
        std::move(view).backend().ref();
    }
#elif VSL_TYPED_CASE == 21
    void rejected(std::shared_ptr<vsl::impl::BufferImpl> data) {
        vsl::command::BindVertexBufferAt command(0, data);
    }
#elif VSL_TYPED_CASE == 22
    void rejected(const vsl::command::BindVertexBufferAt& command) {
        command.source();
    }
#elif VSL_TYPED_CASE == 23
    void rejected(const t::TypedVertexBuffer<Vertex>& buffer) {
        buffer.storage_view().ref();
    }
#endif
} // namespace
