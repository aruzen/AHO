#include <AHO/resource/typed_buffer.hpp>
#include <VSL/utils/typed_pipeline.hpp>
#include <string>

#ifndef VSL_TYPED_CASE
#define VSL_TYPED_CASE 0
#endif

namespace {
    namespace r = aho::resource;
    namespace t = vsl::typed;
    using Host = r::Buffer<r::BufferType::GPU_FRIENDLY>;
    using Device = r::Buffer<r::BufferType::ON_GPU>;
    using Native = vsl::Buffer<vsl::MemoryType::TRANSFER_SOURCE, vsl::MemoryProperty::HOST_VISIBLE>;
    using Transfer = r::Buffer<r::BufferType::CUSTOM, Native>;
    struct Vertex {
        float xy[2];
    };
    struct OtherVertex {
        float xy[2];
    };
    struct Nontrivial {
        std::string text;
    };
    using Record = t::VertexRecord<Vertex, VSL_VERTEX_MEMBER(Vertex, xy)>;
    using Pipeline = t::TypedGraphicsPipeline<t::TypedVertexInput<Record::Binding<0>>>;
    using Set = t::DescriptorSet<0, t::UniformBinding<0, std::uint32_t, vsl::ShaderFlag::VERTEX>>;
    static_assert(std::same_as<decltype(r::vertex_view<Vertex>(std::declval<const Device&>())), t::TypedVertexBufferView<Vertex>>);
    static_assert(std::same_as<decltype(r::uniform_view<std::uint32_t>(std::declval<const Host&>())), t::TypedResourceBufferView<std::uint32_t, vsl::graphic_resource::Type::UNIFORM_BUFFER>>);

    [[maybe_unused]] void contract(Host& host, Device& gpu, Transfer& transfer, r::Buffer<r::BufferType::GENERAL>& cpu, t::TypedDescriptorSet<Set>& set, Pipeline& pipeline) {
#if VSL_TYPED_CASE == 1
        r::uniform_view<std::uint32_t>(transfer);
#elif VSL_TYPED_CASE == 2
        r::vertex_view<Vertex>(transfer);
#elif VSL_TYPED_CASE == 3
        r::vertex_view<Vertex>(cpu);
#elif VSL_TYPED_CASE == 4
        set.write<0>(r::storage_view<std::uint32_t>(host));
#elif VSL_TYPED_CASE == 5
        set.write<0>(r::uniform_view<float>(host));
#elif VSL_TYPED_CASE == 6
        pipeline.bind_vertices(r::vertex_view<OtherVertex>(host));
#elif VSL_TYPED_CASE == 7
        r::vertex_view<Nontrivial>(host);
#else
        set.write<0>(r::uniform_view<std::uint32_t>(host, 0, 1));
        pipeline.bind_vertices(r::vertex_view<Vertex>(host)).draw(3);
        pipeline.bind_vertices(r::vertex_view<Vertex>(gpu)).draw(3);
#endif
    }
} // namespace
