#include <AHO/drawer/standard_drawer.hpp>
#include <AHO/drawer/draw_phase.hpp>
#include <VSL/utils/typed_pipeline.hpp>
#include <VSL/vulkan/stages/color_blend.hpp>
#include <VSL/vulkan/stages/shader_group.hpp>
#include <VSL/vulkan/stages/input_assembly.hpp>
#include <VSL/vulkan/stages/rasterization.hpp>
#include <VSL/vulkan/stages/multisample.hpp>
#include <VSL/vulkan/stages/dynamic_state.hpp>
#include <VSL/vulkan/viewport.hpp>
#include <VSL/vulkan/scissor.hpp>
#include "standard_shaders.hpp"
#include <cmath>
#include <stdexcept>

namespace AHO_NAMESPACE::drawer::detail {
    struct TriangleValues {
        float position1[4], position2[4], position3[4], color[4];
    };
    using TriangleLayout = VSL_NAMESPACE::typed::BlockRecord<TriangleValues,
                                                             VSL_BLOCK_MEMBER(TriangleValues, position1), VSL_BLOCK_MEMBER(TriangleValues, position2),
                                                             VSL_BLOCK_MEMBER(TriangleValues, position3), VSL_BLOCK_MEMBER(TriangleValues, color)>;
    using TrianglePush = VSL_NAMESPACE::typed::PushSchema<VSL_NAMESPACE::typed::PushRange<TriangleLayout, VSL_NAMESPACE::ShaderFlag::VERTEX>>;
    using TrianglePipeline = VSL_NAMESPACE::typed::TypedGraphicsPipeline<VSL_NAMESPACE::typed::TypedVertexInput<>,
                                                                         VSL_NAMESPACE::typed::ResourceSchema<>, TrianglePush>;

    struct StandardDrawState {
        VSL_NAMESPACE::RenderPass pass;
        TrianglePipeline pipeline;
        explicit StandardDrawState(const RenderContext& context);
        static TrianglePipeline MakePipeline(const RenderContext& context, const VSL_NAMESPACE::RenderPass& pass);
    };
    TriangleValues triangle_values(const Triangle<float, VSL_NAMESPACE::D2>& triangle, const RGBA& color);

    StandardDrawState::StandardDrawState(const RenderContext& context)
        : pass(context.render_pass()), pipeline(MakePipeline(context, pass)) {}

    TrianglePipeline StandardDrawState::MakePipeline(const RenderContext& context, const VSL_NAMESPACE::RenderPass& pass) {
        namespace pl = VSL_NAMESPACE::pipeline_layout;
        const VSL_NAMESPACE::Shader<VSL_NAMESPACE::ShaderType::VERTEX> vertex(context.device(), std::span(TRIANGLE_VERT));
        const VSL_NAMESPACE::Shader<VSL_NAMESPACE::ShaderType::FRAGMENT> fragment(context.device(), std::span(TRIANGLE_FRAG));
        pl::ColorBlendAttachment blend;
        blend.destination_alpha = pl::BlendFactor::ONE_MINUS_SRC_ALPHA;
        return TrianglePipeline(context.device(), pass, pl::ShaderGroup("AHO Triangle", {vertex, fragment}),
                                pl::ColorBlend(blend), pl::InputAssembly{}, pl::Rasterization{pl::CullingMode::NONE},
                                pl::Multisample{}, pl::DynamicState{}, VSL_NAMESPACE::Viewport{}, VSL_NAMESPACE::Scissor{});
    }

    TriangleValues triangle_values(const Triangle<float, VSL_NAMESPACE::D2>& triangle, const RGBA& color) {
        TriangleValues values{
            {triangle.pos1.value.___AN1, triangle.pos1.value.___AN2, 0, 1},
            {triangle.pos2.value.___AN1, triangle.pos2.value.___AN2, 0, 1},
            {triangle.pos3.value.___AN1, triangle.pos3.value.___AN2, 0, 1},
            {color.r.value, color.g.value, color.b.value, color.a.value}};
        for (const auto* vector : {values.position1, values.position2, values.position3, values.color})
            for (std::size_t i = 0; i < 4; ++i)
                if (!std::isfinite(vector[i]))
                    throw std::invalid_argument("Triangle coordinates and color must be finite");
        return values;
    }
} // namespace AHO_NAMESPACE::drawer::detail

namespace AHO_NAMESPACE {
    void draw(const Triangle<float, VSL_NAMESPACE::D2>& triangle, const RGBA& rgba) {
        draw(triangle, rgba, THREAD_LOCAL_DRAW_PHASE);
    }

    void draw(const Triangle<float, VSL_NAMESPACE::D2>& triangle, const RGBA& rgba, DrawPhase* phase) {
        if (!phase)
            throw std::logic_error("Triangle draw requires an active DrawPhase");
        draw(triangle, rgba, *phase);
    }

    void draw(const Triangle<float, VSL_NAMESPACE::D2>& triangle, const RGBA& rgba, DrawPhase& phase) {
        if (phase.state != VSL_NAMESPACE::ComputePhase::State::WAIT_SUBMIT)
            throw std::logic_error("Triangle draw requires a recording DrawPhase");
        const auto values = drawer::detail::triangle_values(triangle, rgba);
        const auto context = phase.context();
        auto window = phase.engine->get_window();
        auto& state = window.data2->standard_draw;
        if (!state || state->pass.data_ != context.render_pass().data_)
            state = std::make_shared<drawer::detail::StandardDrawState>(context);
        phase << context.viewport() << context.scissor()
              << state->pipeline.bind_vertices().push_constants(drawer::detail::TrianglePush::Values(values)).draw(3);
    }
} // namespace AHO_NAMESPACE
