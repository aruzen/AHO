#include <VSL/utils/typed_pipeline.hpp>
#include <VSL/utils/typed_compute.hpp>

#ifndef VSL_TYPED_CASE
#define VSL_TYPED_CASE 0
#endif
namespace {
    namespace t = vsl::typed;
    struct Tint {
        float rgba[4];
    };
    struct Wrong {
        float rgba[4];
    };
    using Push = t::PushSchema<t::PushRange<Tint, vsl::ShaderFlag::VERTEX, 16>>;
    using Other = t::PushSchema<t::PushRange<Tint, vsl::ShaderFlag::FRAGMENT, 16>>;
    using Pipeline = t::TypedGraphicsPipeline<t::TypedVertexInput<>, t::ResourceSchema<>, Push>;
    using Compute = t::TypedComputePipeline<t::ResourceSchema<>, Push>;
#if VSL_TYPED_CASE == 0
    static_assert(!std::default_initializable<Push::Values>);
    static_assert(std::default_initializable<t::PushSchema<>::Values>);
    void accepted(Pipeline& pipeline, const Tint& tint) {
        pipeline.bind_vertices().push_constants(Push::Values(tint)).draw(3);
    }
#elif VSL_TYPED_CASE == 1
    void rejected(const Wrong& value) {
        Push::Values values(value);
    }
#elif VSL_TYPED_CASE == 2
    void rejected() {
        Push::Values values;
    }
#elif VSL_TYPED_CASE == 3
    void rejected(const Tint& tint) {
        Push::Values values(tint, tint);
    }
#elif VSL_TYPED_CASE == 4
    void rejected(Pipeline& pipeline) {
        pipeline.bind_vertices().draw(3);
    }
#elif VSL_TYPED_CASE == 5
    void rejected(Pipeline& pipeline, const Other::Values& values) {
        pipeline.bind_vertices().push_constants(values);
    }
#elif VSL_TYPED_CASE == 6
    using Overlap = t::PushSchema<t::PushRange<Tint, vsl::ShaderFlag::VERTEX>, t::PushRange<std::uint32_t, vsl::ShaderFlag::VERTEX, 8>>;
    static_assert(Overlap::COUNT == 2);
#elif VSL_TYPED_CASE == 7
    using Bad = t::PushRange<Tint, vsl::ShaderFlag::VERTEX, 2>;
    static_assert(Bad::RANGE.offset == 2);
#elif VSL_TYPED_CASE == 8
    using Bad = t::PushRange<std::uint8_t, vsl::ShaderFlag::VERTEX>;
    static_assert(Bad::RANGE.size == 1);
#elif VSL_TYPED_CASE == 9
    using Bad = t::PushRange<Tint, vsl::ShaderFlag::ERROR_>;
    static_assert(Bad::RANGE.size == sizeof(Tint));
#elif VSL_TYPED_CASE == 10
    void rejected(Compute& pipeline) {
        pipeline.bind_resources().dispatch(3);
    }
#elif VSL_TYPED_CASE == 11
    void rejected(Compute& pipeline, const Other::Values& values) {
        pipeline.bind_resources().push_constants(values);
    }
#endif
} // namespace
