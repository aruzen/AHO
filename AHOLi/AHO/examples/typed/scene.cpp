#include "scene.hpp"
#include <array>
#include <cmath>
#include <iostream>

namespace typed_example {
    namespace {
        // The first record is deliberately excluded by the vertex view.
        const std::array<Vertex, 5> VERTICES{{{{99, 99}, {0, 0, 0}},
                                              {{-.6f, -.6f}, {1, .2f, .35f}},
                                              {{.6f, -.6f}, {.2f, .8f, 1}},
                                              {{.6f, .6f}, {.4f, 1, .55f}},
                                              {{-.6f, .6f}, {1, .8f, .2f}}}};
        const std::array<std::uint16_t, 6> INDICES{0, 1, 2, 2, 3, 0};
    } // namespace

    SceneFrame::SceneFrame(vsl::LogicalDeviceAccessor device, vsl::graphic_resource::ManagerInterface& descriptors)
        : uniform(device, SceneUniform{{1, 1, 1, 1}, {0, 0, 1, 0}}), set(device, descriptors) {
        set.write<0>(uniform);
    }

    Scene::Scene(aho::Window window, const std::filesystem::path& shaders)
        : _window(std::move(window)), _context(_window.render_context()),
          _vertices(_context.device(), VERTICES), _indices(_context.device(), INDICES),
          _frames(_context.commands(), [&](size_t) { return SceneFrame(_context.device(), _context.descriptor_manager()); }),
          _vertex_shader(_context.device(), shaders / "scene.vert.spv"), _fragment_shader(_context.device(), shaders / "scene.frag.spv") {
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
        _frames.update(phase, [&](SceneFrame& frame) { frame.uniform.copy(std::span(&uniform, 1)); });
        const auto& frame = _frames.get(phase);
        const auto vertices = _vertices.view(1, 4);
        const auto bound = _pipeline->bind_vertices(vertices).bind_resources(frame.set);
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
