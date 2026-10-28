#include "scene.hpp"
#include <VSL/vulkan/stages/shader_group.hpp>
#include <VSL/vulkan/stages/color_blend.hpp>
#include <VSL/vulkan/stages/input_assembly.hpp>
#include <VSL/vulkan/stages/rasterization.hpp>
#include <VSL/vulkan/stages/multisample.hpp>
#include <VSL/vulkan/stages/dynamic_state.hpp>
#include <array>
#include <cmath>
#include <iostream>

namespace typed_example {
    namespace {
        // 先頭レコードは頂点viewの範囲から意図的に除外する。
        const std::array<Vertex, 5> VERTICES{{{{99, 99}, {0, 0, 0}},
                                              {{-.6f, -.6f}, {1, .2f, .35f}},
                                              {{.6f, -.6f}, {.2f, .8f, 1}},
                                              {{.6f, .6f}, {.4f, 1, .55f}},
                                              {{-.6f, .6f}, {1, .8f, .2f}}}};
        const std::array<std::uint16_t, 6> INDICES{0, 1, 2, 2, 3, 0};
    } // namespace

    Scene::Scene(aho::Window window, const std::filesystem::path& shaders)
        : _window(std::move(window)), _context(_window.render_context()),
          _vertices(_context.device(), VERTICES), _indices(_context.device(), INDICES),
          _resources(_context.commands(), _context.descriptor_manager()),
          _vertex_shader(_context.device(), shaders / "scene.vert.spv"), _fragment_shader(_context.device(), shaders / "scene.frag.spv") {
        _resources.initialize_shared([](Bundle::Shared& shared) {
            const PaletteUniform palette{{1, 1, 1, 1}};
            shared.buffer<1, 0>().copy(std::span(&palette, 1));
        });
        const auto& totals = _resources.plan().totals();
        std::cout << "Typed allocation: " << totals.buffers << " buffers, " << totals.descriptor_sets
                  << " sets, " << totals.descriptors << " descriptors, " << totals.payload_bytes << " payload bytes\n";
        rebuild_pipeline();
        _resize_hook = _window.add_plugin<aho::window::WindowResizeHookPlugin>([this](aho::Window*) { rebuild_pipeline(); });
    }

    Scene::~Scene() {
        _window.remove_plugin(_resize_hook);
    }

    void Scene::rebuild_pipeline() {
        const auto format = _context.format();
        if (_pipeline && _format == format)
            return;
        namespace pl = vsl::pipeline_layout;
        _pipeline.emplace(_context.device(), _context.render_pass(),
                          pl::ShaderGroup("Typed example", {_vertex_shader, _fragment_shader}),
                          pl::ColorBlend{}, pl::InputAssembly{}, pl::Rasterization{pl::CullingMode::NONE},
                          pl::Multisample{}, pl::DynamicState{}, vsl::Viewport{}, vsl::Scissor{});
        _format = format;
        ++_pipeline_builds;
    }

    void Scene::record(aho::DrawPhase& phase, float time) {
        const auto extent = _context.scissor();
        if (!extent.width || !extent.height)
            throw std::logic_error("Typed scene requires a nonzero framebuffer extent");
        const SceneUniform uniform{
            {.75f + .25f * std::cos(time), .75f + .25f * std::cos(time + 1), .75f + .25f * std::cos(time + 2), 1},
            {.15f * std::sin(time * .5f), .1f * std::sin(time * .8f), float(extent.width) / float(extent.height), time}};
        _resources.update(phase, [&](SceneFrame& frame) { frame.buffer<0, 0>().copy(std::span(&uniform, 1)); });
        const auto& frame = _resources.get(phase);
        const auto vertices = _vertices.view(1, 4);
        const auto bound = frame.bind_resources(_pipeline->bind_vertices(vertices));
        const DrawTransform left{{-.48f, 0, .48f, .7f}};
        const DrawTransform right{{.48f, 0, .48f, -.9f}};
        auto batch = t::TypedBatch(
            bound.push_constants(Pushes::Values(left)).draw_indexed(_indices, INDICES.size()),
            bound.push_constants(Pushes::Values(right)).draw_indexed(_indices, INDICES.size()));
        if (time == 0)
            std::cout << "Typed batch: " << batch.statistics().draws << " draws, "
                      << batch.statistics().pipeline_binds << " pipeline, "
                      << batch.statistics().vertex_binds << " vertex, "
                      << batch.statistics().index_binds << " index, "
                      << batch.statistics().descriptor_binds << " descriptor binds\n";
        phase << _context.viewport() << _context.scissor() << batch;
    }
} // namespace typed_example
