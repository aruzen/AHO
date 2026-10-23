#include <VSL/utils/typed_batch.hpp>
#ifndef VSL_TYPED_CASE
#define VSL_TYPED_CASE 0
#endif
namespace {
    namespace t = vsl::typed;
    using Draw = t::TypedDraw<t::TypedVertexInput<>>;
    using Indexed = t::TypedIndexedDraw<t::TypedVertexInput<>>;
    using Dispatch = t::TypedDispatch<t::ResourceSchema<>, t::PushSchema<>>;
#if VSL_TYPED_CASE == 0
    static_assert(!std::default_initializable<t::TypedBatch<Draw>>);
    void accepted(Draw draw, Indexed indexed, Dispatch dispatch) {
        t::TypedBatch graphics(draw, indexed);
        t::TypedBatch compute(dispatch, dispatch);
    }
#elif VSL_TYPED_CASE == 1
    static_assert(sizeof(t::TypedBatch<>) > 0);
#elif VSL_TYPED_CASE == 2
    void rejected(Draw draw, Dispatch dispatch) {
        t::TypedBatch mixed(draw, dispatch);
    }
#elif VSL_TYPED_CASE == 3
    void rejected(Draw draw) {
        t::TypedBatch raw(draw, vsl::command::Draw(3));
    }
#elif VSL_TYPED_CASE == 4
    void rejected(Draw draw) {
        t::TypedBatch invalid(draw, 3);
    }
#elif VSL_TYPED_CASE == 5
    void rejected(t::TypedGraphicsPipeline<t::TypedVertexInput<>>& pipeline) {
        t::TypedBatch binding(pipeline.bind_vertices());
    }
#endif
} // namespace
