#include <VSL/utils/typed_pipeline.hpp>
#include <cstddef>

#ifndef VSL_TYPED_CASE
#define VSL_TYPED_CASE 0
#endif

namespace {
    namespace t = vsl::typed;
    using Timing = t::VertexTiming;
    struct Vertex {
        float xy[2];
    };
    struct OtherVertex {
        std::uint32_t xy[2];
    };
    using Attribute = t::VertexInputDefinition<vsl::data_format::FLOAT_RG, 0, offsetof(Vertex, xy)>;
    using Schema = t::VertexRecord<Vertex, VSL_VERTEX_MEMBER(Vertex, xy)>;
    using Binding = Schema::Binding<0>;
    using Input = t::TypedVertexInput<Binding>;
    using Pipeline = t::TypedGraphicsPipeline<Input>;

#if VSL_TYPED_CASE == 0
    static_assert(Input::LAYOUT.binding[0].stride == sizeof(Vertex));
    static_assert(t::compatible_vertex_buffers<Input, t::TypedVertexBuffer<Vertex>>);
    static_assert(!std::convertible_to<t::TypedVertexBuffer<Vertex>, vsl::BufferAccessor&>);
    static_assert(!std::convertible_to<Pipeline, vsl::PipelineAccessor>);
    void accepted(Pipeline& pipeline, t::TypedVertexBuffer<Vertex>& buffer) {
        auto command = pipeline.bind_vertices(buffer).draw(3);
        static_assert(std::same_as<typename decltype(command)::VertexInput, Input>);
    }
#elif VSL_TYPED_CASE == 1
    void rejected(Pipeline& pipeline, t::TypedVertexBuffer<OtherVertex>& buffer) {
        pipeline.bind_vertices(buffer);
    }
#elif VSL_TYPED_CASE == 2
    void rejected(Pipeline& pipeline) {
        pipeline.bind_vertices();
    }
#elif VSL_TYPED_CASE == 3
    void rejected(vsl::LogicalDeviceAccessor device, std::span<const OtherVertex> values) {
        t::TypedVertexBuffer<Vertex> buffer(device, values);
    }
#elif VSL_TYPED_CASE == 4
    using Second = t::VertexBinding<Vertex, 0, Timing::NEXT_VERTEX,
                                    t::VertexInputDefinition<vsl::data_format::FLOAT_RG, 1, 0>>;
    static_assert(t::TypedVertexInput<Binding, Second>::BINDING_COUNT == 2);
#elif VSL_TYPED_CASE == 5
    using Second = t::VertexBinding<Vertex, 1, Timing::NEXT_VERTEX, Attribute>;
    static_assert(t::TypedVertexInput<Binding, Second>::BINDING_COUNT == 2);
#elif VSL_TYPED_CASE == 6
    using Overflow = t::VertexShapeDefinition<0, t::AUTO, Timing::NEXT_VERTEX,
                                              t::VertexInputDefinition<vsl::data_format::FLOAT_RG, 0, UINT32_MAX - 2>>;
    static_assert(t::TypedVertexInput<Overflow>::BINDING_COUNT == 1);
#elif VSL_TYPED_CASE == 7
    using Small = t::VertexShapeDefinition<0, 4, Timing::NEXT_VERTEX, Attribute>;
    static_assert(t::TypedVertexInput<Small>::BINDING_COUNT == 1);
#elif VSL_TYPED_CASE == 8
    using Automatic = t::VertexBinding<Vertex, 0, Timing::NEXT_VERTEX,
                                       t::VertexInputDefinition<vsl::data_format::FLOAT_RG>>;
    static_assert(t::TypedVertexInput<Automatic>::BINDING_COUNT == 1);
#elif VSL_TYPED_CASE == 9
    using Wide = t::VertexShapeDefinition<0, t::AUTO, Timing::NEXT_VERTEX,
                                          t::VertexInputDefinition<vsl::data_format::FLOAT64_RGB, 0>,
                                          t::VertexInputDefinition<vsl::data_format::FLOAT, 1>>;
    static_assert(t::TypedVertexInput<Wide>::BINDING_COUNT == 1);
#elif VSL_TYPED_CASE == 10
    using Undefined = t::VertexShapeDefinition<0, t::AUTO, Timing::NEXT_VERTEX,
                                               t::VertexInputDefinition<vsl::data_format::UNDEFINED>>;
    static_assert(t::TypedVertexInput<Undefined>::BINDING_COUNT == 1);
#elif VSL_TYPED_CASE == 11
    using Bad = t::VertexRecord<Vertex, VSL_VERTEX_MEMBER(Vertex, xy, 0, vsl::data_format::FLOAT_RGBA)>;
    static_assert(Bad::LAYOUT.valid);
#elif VSL_TYPED_CASE == 12
    using Bad = t::VertexRecord<Vertex, VSL_VERTEX_MEMBER(OtherVertex, xy)>;
    static_assert(Bad::LAYOUT.valid);
#elif VSL_TYPED_CASE == 13
    using Bad = Schema::Binding<0, Timing::NEXT_VERTEX, UINT32_MAX>;
    static_assert(t::TypedVertexInput<Bad>::BINDING_COUNT == 1);
#elif VSL_TYPED_CASE == 14
    struct Unsupported {
        void* member;
    };
    using Bad = t::VertexRecord<Unsupported, VSL_VERTEX_MEMBER(Unsupported, member)>;
    static_assert(Bad::LAYOUT.valid);
#elif VSL_TYPED_CASE == 15
    struct Matrix {
        float values[3][3];
    };
    using Bad = t::VertexRecord<Matrix, VSL_VERTEX_MEMBER(Matrix, values)>;
    static_assert(Bad::LAYOUT.valid);
#endif
} // namespace
